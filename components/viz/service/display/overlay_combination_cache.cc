// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_combination_cache.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_macros.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "components/viz/service/display/overlay_proposed_candidate.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayCacheKey::OverlayCacheKey(
    const OverlayProposedCandidate& proposed_candidate) {
  // Rounding the display_rect here matches the behaviour of DRM when testing
  // OverlayCandidates.
  display_rect = gfx::ToRoundedRect(proposed_candidate.candidate.display_rect);
  strategy = proposed_candidate.strategy->GetUMAEnum();
}

OverlayCombinationToTest::OverlayCombinationToTest() = default;
OverlayCombinationToTest::~OverlayCombinationToTest() = default;
OverlayCombinationToTest::OverlayCombinationToTest(
    OverlayCombinationToTest&& other) = default;
OverlayCombinationToTest& OverlayCombinationToTest::operator=(
    OverlayCombinationToTest&& other) = default;

CombinationIdMapper::CombinationIdMapper() = default;
CombinationIdMapper::~CombinationIdMapper() = default;

OverlayCombinationCache::CandidateId CombinationIdMapper::GetCandidateId(
    const OverlayProposedCandidate& proposed_candidate) {
  OverlayCacheKey cache_key(proposed_candidate);
  auto it = candidate_ids_.find(cache_key);
  if (it != candidate_ids_.end()) {
    return it->second;
  }

  // Find the first unclaimed id.
  for (size_t i = 0; i < OverlayCombinationCache::kMaxTrackedCandidates; ++i) {
    if (!claimed_ids_[i]) {
      candidate_ids_.insert({cache_key, i});
      claimed_ids_.set(i);
      return i;
    }
  }
  // All ids are claimed for now.
  return OverlayCombinationCache::kInvalidCandidateId;
}

void CombinationIdMapper::RemoveStaleIds(
    const OverlayCombinationCache::CandidateCombination& stale_candidates) {
  if (stale_candidates.none()) {
    return;
  }
  std::erase_if(candidate_ids_, [&stale_candidates](auto& entry) {
    return stale_candidates[entry.second];
  });
  claimed_ids_ &= ~stale_candidates;
}

void CombinationIdMapper::ClearIds() {
  candidate_ids_.clear();
  claimed_ids_.reset();
}

OverlayCombinationCache::OverlayCombinationCache()
    : id_mapper_(std::make_unique<CombinationIdMapper>()) {
  combo_results_.fill(CombinationResult::kUnknown);
}

OverlayCombinationCache::~OverlayCombinationCache() = default;

OverlayCombinationToTest OverlayCombinationCache::GetOverlayCombinationToTest(
    base::span<OverlayProposedCandidate const> sorted_candidates,
    int max_overlays_considered) {
  DCHECK_LE(max_overlays_considered, static_cast<int>(kMaxTrackedCandidates));

  OverlayCombinationToTest result;

  size_t max_overlays_possible =
      std::min({static_cast<size_t>(max_overlays_considered),
                sorted_candidates.size(), kMaxTrackedCandidates});
  if (max_overlays_possible == 0) {
    return result;
  }

  std::vector<OverlayProposedCandidate> considered_candidates =
      GetConsideredCandidates(sorted_candidates, max_overlays_possible);
  std::vector<CandidateId> considered_ids = GetIds(considered_candidates);
  DCHECK_EQ(considered_candidates.size(), considered_ids.size());

  std::vector<std::pair<CandidateCombination, int>> power_sorted_combinations =
      GetPowerSortedCombinations(considered_candidates, considered_ids);

  for (auto& entry : power_sorted_combinations) {
    const CandidateCombination& combo = entry.first;
    size_t cache_index = combo.to_ulong();

    // Use the best combination that hasn't failed before.
    auto combo_result = combo_results_[cache_index];
    if (combo_result != CombinationResult::kFailed) {
      result.previously_succeeded =
          combo_result == CombinationResult::kPromoted;
      // Add all candidates in this combination to result.candidates_to_test.
      result.candidates_to_test.reserve(combo.count());
      for (auto& proposed_candidate : considered_candidates) {
        // We know a valid id already exists for this candidate.
        if (combo[id_mapper_->GetCandidateId(proposed_candidate)]) {
          result.candidates_to_test.push_back(proposed_candidate);
        }
      }
      return result;
    }
  }
  // If every possible combination has failed before, just attempt the single
  // highest power gain candidate. When the set of candidates changes the cache
  // will be updated, and we'll try new combinations again.
  result.candidates_to_test.push_back(considered_candidates.front());
  return result;
}

void OverlayCombinationCache::ClearCache() {
  combo_results_.fill(CombinationResult::kUnknown);
  id_mapper_->ClearIds();
}

std::vector<OverlayProposedCandidate>
OverlayCombinationCache::GetConsideredCandidates(
    base::span<OverlayProposedCandidate const> sorted_candidates,
    size_t max_overlays_possible) {
  std::vector<OverlayProposedCandidate> considered_candidates;

  // Used to prevent testing multiple candidates representing the same DrawQuad.
  std::set<size_t> used_quad_indices;
  for (auto& cand : sorted_candidates) {
    if (considered_candidates.size() == max_overlays_possible) {
      break;
    }

    // Skip candidates whose quads have already been added to the test list. A
    // quad could have an on top and an underlay candidate.
    bool inserted = used_quad_indices.insert(cand.quad_iter.index()).second;
    if (!inserted) {
      continue;
    }
    considered_candidates.push_back(cand);
  }
  return considered_candidates;
}

std::vector<OverlayCombinationCache::CandidateId>
OverlayCombinationCache::GetIds(
    const std::vector<OverlayProposedCandidate>& considered_candidates) {
  std::vector<CandidateId> considered_ids;
  considered_ids.reserve(considered_candidates.size());

  CandidateCombination stale_candidates = id_mapper_->GetClaimedIds();

  for (auto& cand : considered_candidates) {
    CandidateId id = id_mapper_->GetCandidateId(cand);
    considered_ids.push_back(id);
    if (id != kInvalidCandidateId) {
      stale_candidates.reset(id);
    }
  }
  // Remove all cached combinations that contained these candidates.
  RemoveStaleCombinations(stale_candidates);
  // Remove stale candidates from the id mapper.
  id_mapper_->RemoveStaleIds(stale_candidates);

  // Now that we've removed all stale candidates, we will have room to assign
  // ids to all `considered_candidates` members.
  for (size_t i = 0; i < considered_candidates.size(); ++i) {
    if (considered_ids[i] == kInvalidCandidateId) {
      considered_ids[i] = id_mapper_->GetCandidateId(considered_candidates[i]);
      DCHECK_NE(considered_ids[i], kInvalidCandidateId);
    }
  }

  return considered_ids;
}

std::vector<std::pair<OverlayCombinationCache::CandidateCombination, int>>
OverlayCombinationCache::GetPowerSortedCombinations(
    const std::vector<OverlayProposedCandidate>& considered_candidates,
    const std::vector<CandidateId>& considered_ids) {
  std::vector<std::pair<CandidateCombination, int>> combination_power_gains;
  const size_t num_candidates = considered_candidates.size();
  const size_t num_combinations = 1 << num_candidates;
  combination_power_gains.reserve(num_combinations - 1);

  // Skip the empty (i = 0) combination
  for (size_t i = 1; i < num_combinations; ++i) {
    CandidateCombination combo;
    int total_power_gain = 0;
    for (size_t bit = 0; bit < num_candidates; ++bit) {
      if (i & (1 << bit)) {
        combo.set(considered_ids[bit]);
        total_power_gain += considered_candidates[bit].relative_power_gain;
      }
    }
    combination_power_gains.emplace_back(
        std::make_pair(combo, total_power_gain));
  }

  // Sort combinations by highest total power gain.
  // Use stable_sort to guarantee the same set of candidates will have the same
  // order from one frame to the next if they tie.
  std::stable_sort(
      combination_power_gains.begin(), combination_power_gains.end(),
      [](const auto& a, const auto& b) { return a.second > b.second; });

  return combination_power_gains;
}

void OverlayCombinationCache::DeclarePromotedCandidates(
    base::span<OverlayProposedCandidate const> attempted_candidates) {
  if (attempted_candidates.empty()) {
    return;
  }

  CandidateCombination attempted_combo;
  CandidateCombination promoted_combo;
  for (auto& proposed_candidate : attempted_candidates) {
    CandidateId id = id_mapper_->GetCandidateId(proposed_candidate);
    if (id == kInvalidCandidateId) {
      // If one of the candidates mapped to an invalid id, then don't
      // update the cache.
      return;
    }
    attempted_combo.set(id);
    if (proposed_candidate.candidate.overlay_handled) {
      promoted_combo.set(id);
    }
  }
  size_t attempted_combo_index = attempted_combo.to_ulong();
  size_t promoted_combo_index = promoted_combo.to_ulong();

  if (promoted_combo.any()) {
    combo_results_[promoted_combo_index] = CombinationResult::kPromoted;
  }

  if (attempted_combo_index != promoted_combo_index) {
    combo_results_[attempted_combo_index] = CombinationResult::kFailed;
  }
}

void OverlayCombinationCache::RemoveStaleCombinations(
    const CandidateCombination& stale_candidates) {
  if (stale_candidates.none()) {
    return;
  }
  for (size_t i = 0; i < combo_results_.size(); ++i) {
    // If any known combo results contain any stale candidates, then remove
    // those cached results.
    if (combo_results_[i] != CombinationResult::kUnknown) {
      bool is_stale = i & stale_candidates.to_ulong();
      if (is_stale) {
        combo_results_[i] = CombinationResult::kUnknown;
      }
    }
  }
}

}  // namespace viz
