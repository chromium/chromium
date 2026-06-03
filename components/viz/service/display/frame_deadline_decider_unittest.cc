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

constexpr base::TimeDelta k120HzVsyncInterval = base::Milliseconds(8);
constexpr int k120HzMaxPendingSwaps = 4;

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

  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines, k120HzVsyncInterval,
                                        k120HzMaxPendingSwaps));
}

#if BUILDFLAG(IS_ANDROID)
class AndroidFrameDeadlineDeciderTest : public FrameDeadlineDeciderTest {
 public:
  AndroidFrameDeadlineDeciderTest() = default;
  ~AndroidFrameDeadlineDeciderTest() override = default;
};

TEST_F(AndroidFrameDeadlineDeciderTest, SingleFrameSequenceDefaultOffset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "0"}});

  // Setup 120Hz deadlines.
  // num_buffers = 4 + 1 = 5.
  // Target present multiplier = max(1, 5 + 0) = 5.
  // Target present delta = 5 * 8ms = 40ms.
  // OS preferred = index 0 (present = 16ms).
  // Custom matches index 2 (present = 40ms).
  // Deadlines:
  // index 0: 16ms (OS preferred)
  // index 1: 32ms (before target)
  // index 2: 40ms (custom target)
  // index 3: 48ms (after target)
  auto deadlines = CreatePossibleDeadlines(
      0, {
             PossibleDeadline(1, base::Milliseconds(8),
                              base::Milliseconds(16)),  // OS preferred
             PossibleDeadline(2, base::Milliseconds(24),
                              base::Milliseconds(32)),  // Before target
             PossibleDeadline(3, base::Milliseconds(32),
                              base::Milliseconds(40)),  // Custom target
             PossibleDeadline(4, base::Milliseconds(40),
                              base::Milliseconds(48))  // After target
         });

  EXPECT_EQ(2u, decider_.SelectDeadline(deadlines, k120HzVsyncInterval,
                                        k120HzMaxPendingSwaps));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SingleFrameSequenceNegativeOffset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-1"}});

  // Setup 120Hz deadlines.
  // num_buffers = 4 + 1 = 5.
  // Target present multiplier = max(1, 5 - 1) = 4.
  // Target present delta = 4 * 8ms = 32ms.
  // OS preferred = index 0 (present = 16ms).
  // Custom matches index 2 (present = 32ms).
  // Deadlines:
  // index 0: 16ms (OS preferred)
  // index 1: 24ms (before target)
  // index 2: 32ms (custom target)
  // index 3: 40ms (after target)
  auto deadlines = CreatePossibleDeadlines(
      0, {
             PossibleDeadline(1, base::Milliseconds(8),
                              base::Milliseconds(16)),  // OS preferred
             PossibleDeadline(2, base::Milliseconds(16),
                              base::Milliseconds(24)),  // Before target
             PossibleDeadline(3, base::Milliseconds(24),
                              base::Milliseconds(32)),  // Custom target
             PossibleDeadline(4, base::Milliseconds(32),
                              base::Milliseconds(40))  // After target
         });

  EXPECT_EQ(2u, decider_.SelectDeadline(deadlines, k120HzVsyncInterval,
                                        k120HzMaxPendingSwaps));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SanityGuardFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-4"}});

  // Setup 120Hz deadlines.
  // num_buffers = 4 + 1 = 5.
  // Target present multiplier = max(1, 5 - 4) = 1.
  // Target present delta = 1 * 8ms = 8ms.
  // Preferred index = 1 (16ms).
  // Custom target is index 0 (present = 8ms).
  // Since custom presentation (8ms) < native preferred (16ms), sanity guard
  // triggers and falls back to preferred (index 1).
  auto deadlines = CreatePossibleDeadlines(
      1, {
             PossibleDeadline(1, base::Milliseconds(4),
                              base::Milliseconds(8)),  // Custom target
             PossibleDeadline(2, base::Milliseconds(8),
                              base::Milliseconds(16))  // OS preferred
         });

  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines, k120HzVsyncInterval,
                                        k120HzMaxPendingSwaps));
}

TEST_F(AndroidFrameDeadlineDeciderTest, BinarySearchLessThanOrEqualSelection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-1"}});

  // Setup 120Hz deadlines.
  // num_buffers = 4 + 1 = 5.
  // Target present multiplier = max(1, 5 - 1) = 4.
  // Target present delta = 4 * 8ms = 32ms.
  // OS preferred = index 0 (present = 16ms).
  // Elements available (sorted):
  // index 0: present = 16ms
  // index 1: present = 28ms (largest element <= 32ms)
  // index 2: present = 36ms (greater than target 32ms)
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8),
                           base::Milliseconds(16)),  // OS preferred
          PossibleDeadline(2, base::Milliseconds(20),
                           base::Milliseconds(28)),  // LTE custom target
          PossibleDeadline(3, base::Milliseconds(28), base::Milliseconds(36))});

  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines, k120HzVsyncInterval,
                                        k120HzMaxPendingSwaps));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SequenceLockingAndReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "0"}});

  // 1. Start sequence: max_pending_swaps = 4. Target = (4+1)*8 = 40ms.
  // Deadlines: [16ms (pref), 40ms]
  // Should select index 1 (40ms).
  auto deadlines_1 = CreatePossibleDeadlines(
      0, {
             PossibleDeadline(1, base::Milliseconds(8),
                              base::Milliseconds(16)),  // OS preferred
             PossibleDeadline(2, base::Milliseconds(32),
                              base::Milliseconds(40))  // Custom target
         });
  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines_1, k120HzVsyncInterval,
                                        k120HzMaxPendingSwaps));

  // 2. Subsequent frame: max_pending_swaps = 2.
  // Recalculated target would be (2+1)*8 = 24ms.
  // Deadlines: [16ms (pref), 24ms, 40ms]
  // If locked, should select index 2 (40ms) because it is closest to previous
  // (40ms). If recalculated, would select index 1 (24ms).
  auto deadlines_2 = CreatePossibleDeadlines(
      0, {
             PossibleDeadline(1, base::Milliseconds(8),
                              base::Milliseconds(16)),  // OS preferred
             PossibleDeadline(2, base::Milliseconds(16),
                              base::Milliseconds(24)),  // Recalculate target
             PossibleDeadline(3, base::Milliseconds(32),
                              base::Milliseconds(40))  // Lock target
         });
  EXPECT_EQ(2u, decider_.SelectDeadline(deadlines_2, k120HzVsyncInterval, 2));

  // 3. Go idle. This should reset the sequence.
  decider_.OnGoIdle();

  // 4. New frame: max_pending_swaps = 2. Target = 24ms.
  // Deadlines: [16ms (pref), 24ms, 40ms]
  // Should recalculate and select index 1 (24ms).
  EXPECT_EQ(1u, decider_.SelectDeadline(deadlines_2, k120HzVsyncInterval, 2));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace viz
