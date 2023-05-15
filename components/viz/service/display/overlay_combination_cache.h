// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_COMBINATION_CACHE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_COMBINATION_CACHE_H_

#include <bitset>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_proposed_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {

// Uniquely identifies OverlayProposedCandidates as it will be stored in the
// OverlayCombinationCache. An overlay with the same strategy and the same
// rounded display_rect are considered identical.
struct OverlayCacheKey {
  explicit OverlayCacheKey(const OverlayProposedCandidate& candidate);

  bool operator<(const OverlayCacheKey& other) const {
    if (other.strategy != strategy) {
      return strategy < other.strategy;
    } else {
      return display_rect < other.display_rect;
    }
  }

  // The candidate's rounded display_rect.
  gfx::Rect display_rect;
  // The strategy that proposed this candidate.
  OverlayStrategy strategy;
};

// A vector of proposed candidates to try and promote to overlays, as determined
// by the OverlayCombinationCache. There is also a flag that indicates that this
// entire combination has been successfully promoted to overlays the last time
// it was tested.
struct VIZ_SERVICE_EXPORT OverlayCombinationToTest {
  OverlayCombinationToTest();
  ~OverlayCombinationToTest();
  OverlayCombinationToTest(OverlayCombinationToTest&& other);
  OverlayCombinationToTest& operator=(OverlayCombinationToTest&& other);

  // The candidates that should be promoted to overlays.
  std::vector<OverlayProposedCandidate> candidates_to_test;
  // True iff this combination of candidates were successfully promoted to
  // overlays the last time they were tested.
  bool previously_succeeded = false;
};

class CombinationIdMapper;

// Attempts to determine the optimal combination of candidates that we should
// attempt to promote to overlays each frame, based on total expected relative
// power gain, and past success/failure of these combinations being promoted.
class VIZ_SERVICE_EXPORT OverlayCombinationCache {
 public:
  // The maximum number of candidates this cache can track.
  static constexpr size_t kMaxTrackedCandidates = 8;
  // The invalid candidate id.
  static constexpr size_t kInvalidCandidateId = kMaxTrackedCandidates;
  // We track all 2^N combinations of the max N tracked overlay candidates.
  static constexpr size_t kMaxTrackedCombinations = 1 << kMaxTrackedCandidates;

  // Represents a set of CandidateIds.
  using CandidateCombination = std::bitset<kMaxTrackedCandidates>;
  // Represents a bit index used in a CandidateCombination.
  // This may also represent `kInvalidCandidateId`.
  // Valid CandidateIds are in the range [0, kMaxTrackedCandidates).
  using CandidateId = size_t;

  OverlayCombinationCache();
  ~OverlayCombinationCache();

  // Given proposed candidates that are sorted by power gain, and the maximum
  // number of overlays you'd like to try and promote, determines the best
  // combination of overlays to try promoting this frame.
  OverlayCombinationToTest GetOverlayCombinationToTest(
      base::span<OverlayProposedCandidate const> sorted_candidates,
      int max_overlays_considered);

  // Records the success/failure of each candidate we attempted to promote to an
  // overlay. This should be called after testing the combination returned by
  // GetOverlayCombinationToTest() each frame.
  // NOTE: The OverlayProposedCandidate.candidate.overlay_handled field should
  // be true iff that candidate was promoted this frame.
  void DeclarePromotedCandidates(
      base::span<OverlayProposedCandidate const> attempted_candidates);

  // Clears all caches. This includes CandidateIds and CombinationResults.
  void ClearCache();

 private:
  friend class OverlayCombinationCacheTest;

  // The result of attempting to promote a CandidateCombination to overlays, or
  // unknown.
  enum class CombinationResult : uint8_t { kUnknown, kPromoted, kFailed };

  // Get the first max_overlays_possible candidates (if there are that many)
  // that represent unique quads. If there are multiple candidates for a single
  // quad, only the first one will be used.
  std::vector<OverlayProposedCandidate> GetConsideredCandidates(
      base::span<OverlayProposedCandidate const> sorted_candidates,
      size_t max_overlays_possible);

  // Gets the CandidateIds for each considered candidate. Reuses cached ids if
  // they exist, or maps new ids. All ids returned will be valid.
  // Any candidates that were considered last frame but are not this frame are
  // considered stale, and will have their CandidateIds and CombinationResults
  // cleared.
  std::vector<CandidateId> GetIds(
      const std::vector<OverlayProposedCandidate>& considered_candidates);

  // Generates all (2^N - 1) possible combinations of considered_candidates
  // (ignoring the empty set), and calculates the sum of their relative power
  // gains. Returns a vector of all of these CandidateCombination and total
  // power gain pairs.
  std::vector<std::pair<CandidateCombination, int>> GetPowerSortedCombinations(
      const std::vector<OverlayProposedCandidate>& considered_candidates,
      const std::vector<CandidateId>& considered_ids);

  // Reset the cached CombinationResult to kUnknown for any combination
  // containing any of the candidates in stale_candidates.
  void RemoveStaleCombinations(const CandidateCombination& stale_candidates);

  // Maps OverlayProposedCandidates to CandidateIds.
  std::unique_ptr<CombinationIdMapper> id_mapper_;
  // The test result, if known, for every possible CandidateCombination.
  std::array<CombinationResult, kMaxTrackedCombinations> combo_results_;
};

// Manages the mapping from OverlayProposedCandidates to CandidateIds ids based
// on their OverlayCacheKey. There are a fixed number of available ids, based on
// the maximum size of a CandidateCombination.
class VIZ_SERVICE_EXPORT CombinationIdMapper {
 public:
  CombinationIdMapper();
  ~CombinationIdMapper();

  // Gets a CandidateId for a given OverlayProposedCandidate.
  // Returns the cached id for this candidate if it exists, otherwise it
  // attempts to map the candidate to new id. If the cache is full (more than
  // kMaxTrackedCandidates in the cache), then kInvalidCandidateId is returned.
  //
  // NOTE: Callers need to check for kInvalidCandidateId before using it to
  // access a CandidateCombination.
  OverlayCombinationCache::CandidateId GetCandidateId(
      const OverlayProposedCandidate& candidate);

  // Returns a CandidateCombination of all the ids currently claimed.
  OverlayCombinationCache::CandidateCombination GetClaimedIds() {
    return claimed_ids_;
  }

  // Remove all ids in stale_candidates from the cache.
  void RemoveStaleIds(
      const OverlayCombinationCache::CandidateCombination& stale_candidates);

  // Clear all cached ids.
  void ClearIds();

 private:
  // Maps up to (kMaxTrackedCandidates) OverlayCacheKeys to unique CandidateIds.
  std::map<OverlayCacheKey, OverlayCombinationCache::CandidateId>
      candidate_ids_;
  // A set containing all ids that have been claimed.
  OverlayCombinationCache::CandidateCombination claimed_ids_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_COMBINATION_CACHE_H_
