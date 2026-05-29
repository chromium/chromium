// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class FrameDeadlineDeciderTest : public testing::Test {
 public:
  FrameDeadlineDeciderTest() = default;
  ~FrameDeadlineDeciderTest() override = default;

 protected:
  FrameDeadlineDecider decider_;
};

PossibleDeadlines CreatePossibleDeadlines(
    size_t preferred_index,
    std::vector<PossibleDeadline> deadlines) {
  PossibleDeadlines possible_deadlines(preferred_index);
  possible_deadlines.deadlines = std::move(deadlines);
  return possible_deadlines;
}

TEST_F(FrameDeadlineDeciderTest, FeatureDisabledFallback) {
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kUseAndroidCustomFrameDeadlines);
#endif

  auto deadlines = CreatePossibleDeadlines(
      1, {PossibleDeadline(1, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(2, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(3, base::Milliseconds(12), base::Milliseconds(20))});

  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines));
}

#if BUILDFLAG(IS_ANDROID)
class AndroidFrameDeadlineDeciderTest : public FrameDeadlineDeciderTest {
 public:
  AndroidFrameDeadlineDeciderTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kUseAndroidCustomFrameDeadlines);
  }
  ~AndroidFrameDeadlineDeciderTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AndroidFrameDeadlineDeciderTest, SingleFrameSequence) {
  // Sequence 1: locks to index 0 (preferred).
  auto deadlines_1 = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(2, base::Milliseconds(8), base::Milliseconds(16))});
  EXPECT_EQ(0u, decider_.SelectDeadline(deadlines_1));

  // Go idle immediately.
  decider_.OnGoIdle();

  // Sequence 2: locks to index 1 (new preferred).
  auto deadlines_2 = CreatePossibleDeadlines(
      1, {PossibleDeadline(3, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(4, base::Milliseconds(8), base::Milliseconds(16))});
  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines_2));
}

TEST_F(AndroidFrameDeadlineDeciderTest, MultipleFrameSequence) {
  // First frame: Preferred is index 1 (present = 16ms).
  // Should lock to 16ms target present delta, and target index 1.
  auto deadlines_1 = CreatePossibleDeadlines(
      1, {PossibleDeadline(1, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(2, base::Milliseconds(8),
                           base::Milliseconds(16)),  // OS preferred
          PossibleDeadline(3, base::Milliseconds(12), base::Milliseconds(20))});
  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines_1));

  // Subsequent frame 1: Preferred changes to index 0 (present = 12ms).
  // But we should match locked 16ms.
  // Available:
  // - index 0 (Preferred): present = 12ms (further from 16ms)
  // - index 1: present = 15ms (within 1ms of 16ms) -> should be selected via
  // cache
  // - index 2: present = 20ms (further from 16ms)
  auto deadlines_2 = CreatePossibleDeadlines(
      0, {PossibleDeadline(4, base::Milliseconds(4),
                           base::Milliseconds(12)),  // OS preferred
          PossibleDeadline(5, base::Milliseconds(8), base::Milliseconds(15)),
          PossibleDeadline(6, base::Milliseconds(12), base::Milliseconds(20))});
  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines_2));

  // Subsequent frame 2: deadlines shift further, cache won't match.
  // Target is still 16ms. Cached index is still 1.
  // Available:
  // - index 0: present = 8ms
  // - index 1 (Preferred): present = 12ms (diff 4ms > 1ms, cache miss)
  // - index 2: present = 16ms (exact match) -> should be selected via full
  // search
  auto deadlines_3 = CreatePossibleDeadlines(
      1, {PossibleDeadline(7, base::Milliseconds(4), base::Milliseconds(8)),
          PossibleDeadline(8, base::Milliseconds(8),
                           base::Milliseconds(12)),  // OS preferred
          PossibleDeadline(9, base::Milliseconds(12), base::Milliseconds(16))});
  EXPECT_EQ(2u, decider_.SelectDeadline(deadlines_3));

  // Go idle.
  decider_.OnGoIdle();

  // Next frame starts new sequence.
  // Preferred is index 2 (present = 20ms). Should lock to 20ms.
  auto deadlines_4 = CreatePossibleDeadlines(
      2,
      {
          PossibleDeadline(10, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(11, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(12, base::Milliseconds(12),
                           base::Milliseconds(20))  // OS preferred
      });
  EXPECT_EQ(2u, decider_.SelectDeadline(deadlines_4));
}

TEST_F(AndroidFrameDeadlineDeciderTest, MultipleFrameSequenceDriftTracking) {
  // First frame: Preferred is index 1 (present = 16ms).
  auto deadlines_1 = CreatePossibleDeadlines(
      1, {PossibleDeadline(1, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(2, base::Milliseconds(8),
                           base::Milliseconds(16)),  // OS preferred
          PossibleDeadline(3, base::Milliseconds(12), base::Milliseconds(20))});
  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines_1));

  // Subsequent frame 1: Matches 14ms (Cache miss!), drifting the tracked target
  // to 14ms.
  auto deadlines_2 = CreatePossibleDeadlines(
      0, {PossibleDeadline(4, base::Milliseconds(4),
                           base::Milliseconds(12)),  // OS preferred
          PossibleDeadline(10, base::Milliseconds(6), base::Milliseconds(13)),
          PossibleDeadline(5, base::Milliseconds(8), base::Milliseconds(14)),
          PossibleDeadline(6, base::Milliseconds(12), base::Milliseconds(20))});
  EXPECT_EQ(2u, decider_.SelectDeadline(deadlines_2));

  // Subsequent frame 2: Drifted target (14ms) vs Locked target (16ms).
  // Available:
  // - index 0: present = 13ms (diff from 14ms = 1ms, from 16ms = 3ms)
  // - index 1: present = 9ms
  // - index 2: present = 11ms
  // - index 3: present = 14ms (diff from 14ms = 0ms, from 16ms = 2ms) ->
  // closest to 14ms (selected via search)
  // - index 4: present = 17ms (diff from 14ms = 3ms, from 16ms = 1ms) ->
  // closest to 16ms
  auto deadlines_3 = CreatePossibleDeadlines(
      0, {PossibleDeadline(7, base::Milliseconds(4), base::Milliseconds(13)),
          PossibleDeadline(11, base::Milliseconds(5), base::Milliseconds(9)),
          PossibleDeadline(12, base::Milliseconds(6), base::Milliseconds(11)),
          PossibleDeadline(8, base::Milliseconds(8),
                           base::Milliseconds(14)),  // OS preferred
          PossibleDeadline(9, base::Milliseconds(12), base::Milliseconds(17))});
  // Since the implementation updates/drifts the tracked target present delta on
  // every frame, it targets 14ms here and selects index 3.
  EXPECT_EQ(3u, decider_.SelectDeadline(deadlines_3));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace viz
