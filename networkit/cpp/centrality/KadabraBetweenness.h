/*
 * KadabraBetweenness.h
 *
 * Created on: 18.07.2018
 * 		 Author: Eugenio Angriman, Alexander van der Grinten
 */

#ifndef KADABRA_H_
#define KADABRA_H_

#include <atomic>
#include <random>
#include <vector>

#include "../auxiliary/SortedList.h"
#include "../base/Algorithm.h"
#include "../components/ConnectedComponents.h"
#include "../graph/Graph.h"

namespace NetworKit {

class StateFrame {
public:
	StateFrame(const count size) { apx.assign(size, 0); };
	count nPairs = 0;
	count epoch = 0;
	std::vector<count> apx;

	void reset(count newEpoch) {
		std::fill(apx.begin(), apx.end(), 0);
		nPairs = 0;
		epoch = newEpoch;
	}
};

class Status {
public:
	Status(const count k);
	const count k;
	std::vector<node> top;
	std::vector<double> approxTop;
	std::vector<bool> finished;
	std::vector<double> bet;
	std::vector<double> errL;
	std::vector<double> errU;
};

class SpSampler {
public:
	SpSampler(const Graph &G, const ConnectedComponents &cc);
	void randomPath(StateFrame *curFrame);
	StateFrame *frame;
	std::mt19937_64 rng;
	std::uniform_int_distribution<node> distr;

private:
	const Graph &G;
	std::vector<uint8_t> timestamp;
	uint8_t globalTS = 1;
	static constexpr uint8_t stampMask = 0x7F;
	static constexpr uint8_t ballMask = 0x80;
	std::vector<count> dist;
	std::vector<count> nPaths;
	std::vector<node> q;
	const ConnectedComponents &cc;
	std::vector<std::pair<node, node>> spEdges;

	inline node randomNode() const;
	void backtrackPath(const node source, const node target, const node start);
	void resetSampler(const count endQ);
	count getDegree(const Graph &graph, node y, bool useDegreeIn);
};

/**
 * @ingroup centrality
 * Approximation of the betweenness centrality and computation of the top-k
 * nodes with highest betweenness centrality according to the algorithm
 * described in Borassi M. and Natale M. (2016): KADABRA is an ADaptive
 * Algorithm for Betweenness via Random Approximation.
 */
class KadabraBetweenness : public Algorithm {

public:
	/**
	 * If k = 0 the algorithm approximates the betweenness centrality of all
	 * vertices of the graph so that the scores are within an additive error @a
	 * err with probability at least (1 - @a delta). Otherwise, the algorithm
	 * computes the exact ranking of the top-k nodes with highest betweenness
	 * centrality.
	 * The algorithm relies on an adaptive random sampling technique of shortest
	 * paths and the number of samples in the worst case is w = ((log(D - 2) +
	 * log(2/delta))/err^2 samples, where D is the diameter of the graph.
	 * Thus, the worst-case performance is O(w * (|E| + |V|)), but performs better
	 * in practice.
	 * NB: in order to work properly the Kadabra algorithm requires a random seed
	 * to be previously set with 'useThreadId' set to true.
	 *
	 * @param G     the graph
	 * @param err   maximum additive error guaranteed when approximating the
	 *              betweenness centrality of all nodes.
	 * @param delta probability that the values of the betweenness centrality are
	 *              within the error guarantee.
	 * @param k     the number of top-k nodes to be computed. Set it to zero to
	 *              approximate the betweenness centrality of all the nodes.
	 * @param unionSample, startFactor algorithm parameters that are automatically
	 *              chosen.
	 */
	KadabraBetweenness(const Graph &G, const double err = 0.01,
	                   const double delta = 0.1, const bool deterministic = false,
	                   const count k = 0, count unionSample = 0,
	                   const count startFactor = 100);

	/**
	 * Executes the Kadabra algorithm.
	 */
	void run() override;

	/**
	 * @return The ranking of the nodes according to their approximated
	 * betweenness centrality.
	 */
	std::vector<std::pair<node, double>> ranking() const;

	/**
	 * @return Nodes of the graph sorted by their approximated betweenness
	 * centrality.
	 */
	std::vector<node> topkNodesList() const {
		assureFinished();
		return topkNodes;
	}

	/**
	 * @return Sorted list of approximated betweenness centrality scores.
	 */
	std::vector<double> topkScoresList() const {
		assureFinished();
		return topkScores;
	}

	/**
	 * @return Approximated betweenness centrality score of all the nodes of the
	 * graph.
	 */
	std::vector<double> scores() const {
		assureFinished();
		return approxSum;
	}

	/**
	 * @return Total number of samples.
	 */
	count getNumberOfIterations() const {
		assureFinished();
		return nPairs;
	}

	/**
	 * @return Upper bound to the number of samples.
	 */
	double getOmega() const {
		assureFinished();
		return omega;
	}

	count maxAllocatedFrames() const {
		assureFinished();
		count maxNFrames = 0;
		for (auto nFrames : maxFrames) {
			maxNFrames = std::max(nFrames, maxNFrames);
		}
		return maxNFrames;
	}

	double diamTime, firstPartTime, parRedTime, dGuessTime, secondPartTime,
	    initialization, finalization;
	uint32_t itersPerStep = 11;

protected:
	const Graph &G;
	const double delta, err;
	const bool deterministic;
	const count k, startFactor;
	count unionSample;
	count nPairs;
	const bool absolute;
	double deltaLMinGuess, deltaUMinGuess, omega;
	std::atomic<int32_t> epochToRead;
	int32_t epochRead;
	count seed0, seed1;
	std::vector<count> maxFrames;

	std::vector<node> topkNodes;
	std::vector<double> topkScores;
	std::vector<std::pair<node, double>> rankingVector;
	std::vector<std::atomic<StateFrame *>> epochFinished;
	std::vector<SpSampler> samplerVec;
	Aux::SortedList *top;
	ConnectedComponents *cc;

	std::vector<double> approxSum;
	std::vector<double> deltaLGuess;
	std::vector<double> deltaUGuess;

	std::atomic<bool> stop;

	void init();
	void computeDeltaGuess();
	void computeBetErr(Status *status, std::vector<double> &bet,
	                   std::vector<double> &errL,
	                   std::vector<double> &errU) const;
	bool computeFinished(Status *status) const;
	void getStatus(Status *status, const bool parallel = false) const;
	void computeApproxParallel(const std::vector<StateFrame> &firstFrames);
	double computeF(const double btilde, const count iterNum,
	                const double deltaL) const;
	double computeG(const double btilde, const count iterNum,
	                const double deltaU) const;
	void fillResult();
	void checkConvergence(Status &status);

	void fillPQ() {
		for (count i = 0; i < G.upperNodeIdBound(); ++i) {
			top->insert(i, approxSum[i]);
		}
	}
};

inline std::vector<std::pair<node, double>>
KadabraBetweenness::ranking() const {
	assureFinished();
	std::vector<std::pair<node, double>> result(topkNodes.size());
#pragma omp parallel for
	for (omp_index i = 0; i < static_cast<omp_index>(result.size()); ++i) {
		result[i] = std::make_pair(topkNodes[i], topkScores[i]);
	}
	return result;
}
} // namespace NetworKit

#endif /* ifndef KADABRA_H_ */
