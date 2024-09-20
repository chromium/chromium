// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_combination_cache.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/display/overlay_proposed_candidate.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using CandidateCombination = viz::OverlayCombinationCache::CandidateCombination;
using CandidateId = viz::OverlayCombinationCache::CandidateId;

namespace viz {
namespace {

// We only need this class to pass a pointer to OverlayStrategySingleOnTop.
class TestOverlayProcessor : public OverlayProcessorUsingStrategy {
  bool IsOverlaySupported() const override { return true; }
  bool NeedsSurfaceDamageRectList() const override { return false; }
  void CheckOverlaySupportImpl(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
      OverlayCandidateList* surfaces) override {}
  size_t GetStrategyCount() const { return 1; }
};

}  // namespace

class OverlayCombinationCacheTest : public testing::Test {
 public:
  OverlayCombinationCacheTest()
      : on_top_strategy_(&overlay_processor_),
        underlay_strategy_(&overlay_processor_) {}

 protected:
  // Access private methods on OverlayCombinationCache for testing.
  std::vector<OverlayProposedCandidate> GetConsideredCandidates(
      base::span<OverlayProposedCandidate const> sorted_candidates,
      size_t max_overlays_possible) {
    return combination_cache_.GetConsideredCandidates(sorted_candidates,
                                                      max_overlays_possible);
  }
  std::vector<CandidateId> GetIds(
      const std::vector<OverlayProposedCandidate>& considered_candidates) {
    return combination_cache_.GetIds(considered_candidates);
  }
  std::vector<std::pair<CandidateCombination, int>> GetPowerSortedCombinations(
      const std::vector<OverlayProposedCandidate>& considered_candidates,
      const std::vector<CandidateId>& considered_ids) {
    return combination_cache_.GetPowerSortedCombinations(considered_candidates,
                                                         considered_ids);
  }

  // Create N candidates with power gains from N..1
  std::vector<OverlayProposedCandidate> MakeCandidates(int num_candidates) {
    std::vector<int> power_gains;
    int order = num_candidates;
    for (int i = 0; i < num_candidates; ++i) {
      power_gains.push_back(order--);
    }
    return MakeCandidatesWithPowerGains(power_gains);
  }

  std::vector<OverlayProposedCandidate> MakeCandidatesWithPowerGains(
      std::vector<int> power_gains) {
    std::vector<OverlayProposedCandidate> proposed_candidates;

    // Use RenderPassBuilder to easily make a QuadList.
    CompositorRenderPassId id{1};
    RenderPassBuilder pass_builder(id, gfx::Rect(0, 0, 100, 100));
    for (size_t i = 0; i < power_gains.size(); ++i) {
      ResourceId resource_id(i + 1);
      pass_builder.AddTextureQuad(gfx::Rect(0, 0, 10, 10), resource_id);
    }
    render_pass_ = pass_builder.Build();

    auto quad_iter = render_pass_->quad_list.begin();
    for (size_t i = 0; i < power_gains.size(); ++i) {
      OverlayCandidate candidate;
      // Give every candidate a unique position so they get different cache keys
      candidate.display_rect.SetRect(i, 0, 10, 10);
      OverlayProposedCandidate proposed_candidate(quad_iter, candidate,
                                                  &on_top_strategy_);
      proposed_candidate.relative_power_gain = power_gains[i];
      proposed_candidates.push_back(proposed_candidate);

      quad_iter++;
    }
    // Canadidate are supposed to be sorted by relative_power_gain.
    std::sort(proposed_candidates.begin(), proposed_candidates.end(),
              [](const auto& cand1, const auto& cand2) {
                return cand1.relative_power_gain > cand2.relative_power_gain;
              });

    return proposed_candidates;
  }

  void ExpectPowerGains(const std::vector<OverlayProposedCandidate>& result,
                        const std::vector<int>& expected_power_gains) const {
    ASSERT_EQ(result.size(), expected_power_gains.size());
    for (size_t i = 0; i < result.size(); ++i) {
      EXPECT_EQ(result[i].relative_power_gain, expected_power_gains[i]);
    }
  }

  OverlayCombinationCache combination_cache_;
  CombinationIdMapper id_mapper_;
  TestOverlayProcessor overlay_processor_;
  OverlayStrategySingleOnTop on_top_strategy_;
  OverlayStrategyUnderlay underlay_strategy_;
  std::unique_ptr<CompositorRenderPass> render_pass_;
};

TEST_F(OverlayCombinationCacheTest, NoCandidates) {
  std::vector<OverlayProposedCandidate> sorted_candidates;

  auto result =
      combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 2);

  // No candidates of course.
  EXPECT_TRUE(result.candidates_to_test.empty());
  EXPECT_FALSE(result.previously_succeeded);
}

TEST_F(OverlayCombinationCacheTest, AllCombosFail) {
  auto sorted_candidates = MakeCandidatesWithPowerGains({10, 5, 1});

  // Descending list sorted by sum of members. Once all combinations have failed
  // we'll resort to just attempting the highest candidate until a cache update.
  std::vector<std::vector<int>> expectations = {
      {10, 5, 1}, {10, 5}, {10, 1}, {10}, {5, 1}, {5}, {1}, {10}, {10}, {10}};

  for (size_t i = 0; i < expectations.size(); ++i) {
    SCOPED_TRACE(i);
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    ExpectPowerGains(result.candidates_to_test, expectations[i]);
    EXPECT_FALSE(result.previously_succeeded);

    // No candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = false;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
}

TEST_F(OverlayCombinationCacheTest, SomePass) {
  auto sorted_candidates = MakeCandidatesWithPowerGains({10, 5, 1});
  {
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Expect all to be tested
    ExpectPowerGains(result.candidates_to_test, {10, 5, 1});
    EXPECT_FALSE(result.previously_succeeded);

    // All candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = true;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  {  // Same candidates next frame.
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Same combination tested because it passed last time.
    ExpectPowerGains(result.candidates_to_test, {10, 5, 1});
    // This passed last time.
    EXPECT_TRUE(result.previously_succeeded);

    // Only first two pass.
    result.candidates_to_test[0].candidate.overlay_handled = true;
    result.candidates_to_test[1].candidate.overlay_handled = true;
    result.candidates_to_test[2].candidate.overlay_handled = false;
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  {  // Same candidates next frame.
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Next best combination tested.
    ExpectPowerGains(result.candidates_to_test, {10, 5});
    // This passed last time.
    EXPECT_TRUE(result.previously_succeeded);

    // Both fail.
    result.candidates_to_test[0].candidate.overlay_handled = false;
    result.candidates_to_test[1].candidate.overlay_handled = false;
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  {  // Same candidates next frame.
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Next best combination tested.
    ExpectPowerGains(result.candidates_to_test, {10, 1});
    // Not tested before.
    EXPECT_FALSE(result.previously_succeeded);
  }
}

TEST_F(OverlayCombinationCacheTest, CandidatesChange) {
  auto sorted_candidates = MakeCandidatesWithPowerGains({10, 5, 1});
  {
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Expect all to be tested
    ExpectPowerGains(result.candidates_to_test, {10, 5, 1});
    // Not tested before.
    EXPECT_FALSE(result.previously_succeeded);

    // All candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = true;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  {
    // First candidate has less power savings this frame, but candidates are the
    // same.
    sorted_candidates = MakeCandidatesWithPowerGains({3, 5, 1});
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Expect all to be tested, but order is different.
    ExpectPowerGains(result.candidates_to_test, {5, 3, 1});
    // This is the same combination that passed last frame.
    EXPECT_TRUE(result.previously_succeeded);

    // No candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = false;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  {
    // Same candidates this frame.
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Next best combo tested.
    ExpectPowerGains(result.candidates_to_test, {5, 3});
    // Not tested before.
    EXPECT_FALSE(result.previously_succeeded);

    // All candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = true;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  {
    // Third candidate moves a little this frame.
    sorted_candidates[2].candidate.display_rect.set_y(2);
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // That inavlidated cache results containing the third candidate, so now the
    // combination of all candidates gets tested again.
    ExpectPowerGains(result.candidates_to_test, {5, 3, 1});
    // Not tested before.
    EXPECT_FALSE(result.previously_succeeded);

    // No candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = false;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  {
    // Same candidates this frame.
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Next best combo tested.
    ExpectPowerGains(result.candidates_to_test, {5, 3});
    // This was tested before, because the third candidate moving didn't
    // invalidate this cache entry.
    EXPECT_TRUE(result.previously_succeeded);
  }
}

TEST_F(OverlayCombinationCacheTest, ClearCache) {
  auto sorted_candidates = MakeCandidatesWithPowerGains({10, 5, 1});
  {
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Expect all to be tested
    ExpectPowerGains(result.candidates_to_test, {10, 5, 1});
    // Not tested before.
    EXPECT_FALSE(result.previously_succeeded);

    // No candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = false;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  combination_cache_.ClearCache();
  {
    // Same candidates this frame.
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Same combination tested because the cache was cleared.
    ExpectPowerGains(result.candidates_to_test, {10, 5, 1});
    // Not tested before.
    EXPECT_FALSE(result.previously_succeeded);

    // All candidates promoted.
    for (auto& proposed_candidate : result.candidates_to_test) {
      proposed_candidate.candidate.overlay_handled = true;
    }
    combination_cache_.DeclarePromotedCandidates(result.candidates_to_test);
  }
  combination_cache_.ClearCache();
  {
    // Same candidates this frame.
    auto result =
        combination_cache_.GetOverlayCombinationToTest(sorted_candidates, 3);

    // Best combination tested.
    ExpectPowerGains(result.candidates_to_test, {10, 5, 1});
    // This succeeded last frame, but the cache was cleared so this is false.
    EXPECT_FALSE(result.previously_succeeded);
  }
}

TEST_F(OverlayCombinationCacheTest, GetConsideredCandidatesUnique) {
  // All 3 candidates will be unique
  auto sorted_candidates = MakeCandidates(3);

  auto considered_candidates = GetConsideredCandidates(sorted_candidates, 4);
  ExpectPowerGains(considered_candidates, {3, 2, 1});

  considered_candidates = GetConsideredCandidates(sorted_candidates, 3);
  ExpectPowerGains(considered_candidates, {3, 2, 1});

  considered_candidates = GetConsideredCandidates(sorted_candidates, 2);
  ExpectPowerGains(considered_candidates, {3, 2});

  considered_candidates = GetConsideredCandidates(sorted_candidates, 1);
  ExpectPowerGains(considered_candidates, {3});

  considered_candidates = GetConsideredCandidates(sorted_candidates, 0);
  ExpectPowerGains(considered_candidates, {});
}

TEST_F(OverlayCombinationCacheTest, GetConsideredCandidatesWithDuplicates) {
  auto sorted_candidates = MakeCandidates(2);

  auto considered_candidates = GetConsideredCandidates(sorted_candidates, 2);

  ExpectPowerGains(considered_candidates, {2, 1});
  // Both are using the on top strategy.
  EXPECT_EQ(considered_candidates[0].strategy, &on_top_strategy_);
  EXPECT_EQ(considered_candidates[1].strategy, &on_top_strategy_);

  // Insert a copy at the front of the first candidate with a different
  // strategy.
  OverlayProposedCandidate& front = sorted_candidates.front();
  OverlayProposedCandidate underlay_duplicate(front.quad_iter, front.candidate,
                                              &underlay_strategy_);
  underlay_duplicate.relative_power_gain = front.relative_power_gain;
  sorted_candidates.insert(sorted_candidates.begin(), underlay_duplicate);

  considered_candidates = GetConsideredCandidates(sorted_candidates, 3);

  // Still only 2 candidates considered, because two are for the same quad.
  ExpectPowerGains(considered_candidates, {2, 1});
  // The first one is used, this time the underlay candidate.
  EXPECT_EQ(considered_candidates[0].strategy, &underlay_strategy_);
  EXPECT_EQ(considered_candidates[1].strategy, &on_top_strategy_);
}

TEST_F(OverlayCombinationCacheTest, GetIds) {
  auto considered_candidates = MakeCandidates(2);

  std::vector<CandidateId> ids = GetIds(considered_candidates);
  ASSERT_THAT(ids, ElementsAre(0, 1));

  // Add 2 more candidates. The first two will be identical to the previous two.
  considered_candidates = MakeCandidates(4);
  ids = GetIds(considered_candidates);
  ASSERT_THAT(ids, ElementsAre(0, 1, 2, 3));

  // Reverse the list.
  std::reverse(considered_candidates.begin(), considered_candidates.end());
  ids = GetIds(considered_candidates);
  // Ids are cached, so the ids list is also reverse.
  ASSERT_THAT(ids, ElementsAre(3, 2, 1, 0));

  // Make 6 new candidates.
  considered_candidates = MakeCandidates(6);
  // Move the candidates 1 pixel so they're all different from the last set.
  for (auto& cand : considered_candidates) {
    cand.candidate.display_rect.set_y(1);
  }

  ids = GetIds(considered_candidates);
  // New ids will get used until they run out, then they'll be reused in order.
  ASSERT_THAT(ids, ElementsAre(4, 5, 6, 7, 0, 1));

  // Make 8 candidates.
  considered_candidates = MakeCandidates(8);
  ids = GetIds(considered_candidates);
  // Again, the unused ids will be assigned first, then they're reused in order.
  ASSERT_THAT(ids, ElementsAre(2, 3, 0, 1, 4, 5, 6, 7));
}

TEST_F(OverlayCombinationCacheTest, GetPowerSortedCombinations) {
  auto considered_candidates = MakeCandidatesWithPowerGains({8, 4, 2, 1});
  std::vector<CandidateId> ids = {3, 2, 1, 0};

  std::vector<std::pair<CandidateCombination, int>> power_sorted_combinations =
      GetPowerSortedCombinations(considered_candidates, ids);

  // Power gains and ids were setup such that so the total power gain equals the
  // decimal value of the bitset's binary representation.
  std::vector<std::pair<CandidateCombination, int>> expected{
      std::pair<CandidateCombination, int>("1111", 15),
      std::pair<CandidateCombination, int>("1110", 14),
      std::pair<CandidateCombination, int>("1101", 13),
      std::pair<CandidateCombination, int>("1100", 12),
      std::pair<CandidateCombination, int>("1011", 11),
      std::pair<CandidateCombination, int>("1010", 10),
      std::pair<CandidateCombination, int>("1001", 9),
      std::pair<CandidateCombination, int>("1000", 8),
      std::pair<CandidateCombination, int>("0111", 7),
      std::pair<CandidateCombination, int>("0110", 6),
      std::pair<CandidateCombination, int>("0101", 5),
      std::pair<CandidateCombination, int>("0100", 4),
      std::pair<CandidateCombination, int>("0011", 3),
      std::pair<CandidateCombination, int>("0010", 2),
      std::pair<CandidateCombination, int>("0001", 1),
  };
  EXPECT_THAT(power_sorted_combinations, ElementsAreArray(expected));

  // Use the same candidates but with different ids. Notably they're still in
  // descending order, but more spread out.
  std::vector<CandidateId> different_ids = {7, 5, 2, 0};
  power_sorted_combinations =
      GetPowerSortedCombinations(considered_candidates, different_ids);

  // Same pattern as before, but with bits more spread out.
  expected = {
      std::pair<CandidateCombination, int>("10100101", 15),
      std::pair<CandidateCombination, int>("10100100", 14),
      std::pair<CandidateCombination, int>("10100001", 13),
      std::pair<CandidateCombination, int>("10100000", 12),
      std::pair<CandidateCombination, int>("10000101", 11),
      std::pair<CandidateCombination, int>("10000100", 10),
      std::pair<CandidateCombination, int>("10000001", 9),
      std::pair<CandidateCombination, int>("10000000", 8),
      std::pair<CandidateCombination, int>("00100101", 7),
      std::pair<CandidateCombination, int>("00100100", 6),
      std::pair<CandidateCombination, int>("00100001", 5),
      std::pair<CandidateCombination, int>("00100000", 4),
      std::pair<CandidateCombination, int>("00000101", 3),
      std::pair<CandidateCombination, int>("00000100", 2),
      std::pair<CandidateCombination, int>("00000001", 1),
  };
  EXPECT_THAT(power_sorted_combinations, ElementsAreArray(expected));
}

TEST_F(OverlayCombinationCacheTest, IdMapper) {
  auto candidates = MakeCandidates(10);

  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[0]), 0u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[1]), 1u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[2]), 2u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[3]), 3u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[4]), 4u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[5]), 5u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[6]), 6u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[7]), 7u);
  // There's a maximum of 8 ids.
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[8]),
            OverlayCombinationCache::kInvalidCandidateId);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[9]),
            OverlayCombinationCache::kInvalidCandidateId);
  // Id's are cached.
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[0]), 0u);

  EXPECT_EQ(id_mapper_.GetClaimedIds(), CandidateCombination("11111111"));

  id_mapper_.RemoveStaleIds(CandidateCombination("00000101"));

  EXPECT_EQ(id_mapper_.GetClaimedIds(), CandidateCombination("11111010"));

  // Now there's room these candidates.
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[8]), 0u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[9]), 2u);
  // This candidate's id was stolen.
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[0]),
            OverlayCombinationCache::kInvalidCandidateId);

  EXPECT_EQ(id_mapper_.GetClaimedIds(), CandidateCombination("11111111"));

  id_mapper_.ClearIds();

  EXPECT_EQ(id_mapper_.GetClaimedIds(), CandidateCombination("00000000"));

  // Candidate order is scrambled, but ids are allocated in order
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[7]), 0u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[4]), 1u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[3]), 2u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[0]), 3u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[5]), 4u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[1]), 5u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[6]), 6u);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[9]), 7u);
  // There's a maximum of 8 ids.
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[2]),
            OverlayCombinationCache::kInvalidCandidateId);
  EXPECT_EQ(id_mapper_.GetCandidateId(candidates[8]),
            OverlayCombinationCache::kInvalidCandidateId);
}

}  // namespace viz
