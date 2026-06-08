// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr base::TimeDelta k120HzVsyncInterval = base::Milliseconds(8);
constexpr int k120HzMaxPendingSwaps = 4;
constexpr int k120HzAllowedBuffers = k120HzMaxPendingSwaps + 1;

class FrameDeadlineDeciderTest : public testing::Test {
 public:
  FrameDeadlineDeciderTest() = default;
  ~FrameDeadlineDeciderTest() override = default;
};

PossibleDeadlines CreatePossibleDeadlines(
    size_t os_preferred_index,
    std::vector<PossibleDeadline> deadlines) {
  PossibleDeadlines possible_deadlines(os_preferred_index);
  possible_deadlines.deadlines = std::move(deadlines);
  return possible_deadlines;
}

TEST_F(FrameDeadlineDeciderTest, FeatureDisabledFallback) {
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kUseAndroidCustomFrameDeadlines);
#endif

  FrameDeadlineDecider decider(true);

  auto deadlines = CreatePossibleDeadlines(
      1, {PossibleDeadline(1, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(2, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(3, base::Milliseconds(12), base::Milliseconds(20))});

  EXPECT_EQ(1u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt));
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

  FrameDeadlineDecider decider(false);

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

  EXPECT_EQ(2u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SingleFrameSequenceNegativeOffset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-1"}});

  FrameDeadlineDecider decider(false);

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

  EXPECT_EQ(2u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SanityGuardFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-4"}});

  FrameDeadlineDecider decider(false);

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

  EXPECT_EQ(1u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt));
}

TEST_F(AndroidFrameDeadlineDeciderTest, BinarySearchLessThanOrEqualSelection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-1"}});

  FrameDeadlineDecider decider(false);

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

  EXPECT_EQ(1u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SequenceLockingAndReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "0"}});

  FrameDeadlineDecider decider(false);

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
  EXPECT_EQ(1u, decider.SelectDeadline(deadlines_1, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt));

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
  EXPECT_EQ(2u, decider.SelectDeadline(deadlines_2, k120HzVsyncInterval, 3,
                                       base::TimeTicks(), std::nullopt));

  // 3. Go idle. This should reset the sequence.
  decider.OnGoIdle();

  // 4. New frame: max_pending_swaps = 2. Target = 24ms.
  // Deadlines: [16ms (pref), 24ms, 40ms]
  // Should recalculate and select index 1 (24ms).
  EXPECT_EQ(1u, decider.SelectDeadline(deadlines_2, k120HzVsyncInterval, 3,
                                       base::TimeTicks(), std::nullopt));
}

TEST_F(AndroidFrameDeadlineDeciderTest,
       LatencyCapping_StartOfSequence_SatisfiesTarget) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "0"}});

  FrameDeadlineDecider decider(false);

  // Setup 120Hz deadlines.
  // num_buffers = 4 + 1 = 5.
  // Target present multiplier = max(1, 5 + 0) = 5.
  // Target present delta = 5 * 8ms = 40ms.
  // OS preferred = index 0 (present = 16ms).
  // Custom matches index 2 (present = 40ms).
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8),
                           base::Milliseconds(16)),  // OS preferred
          PossibleDeadline(2, base::Milliseconds(24), base::Milliseconds(32)),
          PossibleDeadline(3, base::Milliseconds(32),
                           base::Milliseconds(40)),  // Custom target
          PossibleDeadline(4, base::Milliseconds(40), base::Milliseconds(48))});

  base::TimeTicks frame_time = base::TimeTicks::Now();
  // Input timestamp is 10ms before frame_time.
  // Input delta = 10ms.
  // Vsync interval = 8ms.
  // Latency cap = 100ms (kPerceptibleLatencyThreshold) - 8ms - 2ms = 90ms.
  // Max present delta = 90ms - 10ms = 80ms.
  // Target present delta from presentation offset 0 = (4 + 1) * 8ms = 40ms.
  // Since target present delta (40ms) <= max present delta (80ms), the target
  // is not reduced. Custom matches index 2 (present = 40ms).
  EXPECT_EQ(2u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, frame_time,
                                       frame_time - base::Milliseconds(10)));
}

TEST_F(AndroidFrameDeadlineDeciderTest,
       LatencyCapping_StartOfSequence_ExceedsTarget_Fallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "0"}});

  FrameDeadlineDecider decider(false);

  // Setup 120Hz deadlines.
  // num_buffers = 4 + 1 = 5.
  // Target present multiplier = max(1, 5 + 0) = 5.
  // Target present delta = 5 * 8ms = 40ms.
  // OS preferred = index 0 (present = 16ms).
  // Deadlines:
  // index 0: 16ms (OS preferred)
  // index 1: 24ms (offset -2)
  // index 2: 32ms (offset -1)
  // index 3: 40ms (offset 0)
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8),
                           base::Milliseconds(16)),  // OS preferred
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(24), base::Milliseconds(32)),
          PossibleDeadline(4, base::Milliseconds(32), base::Milliseconds(40))});

  base::TimeTicks frame_time = base::TimeTicks::Now();
  // Input timestamp is 60ms before frame_time.
  // Input delta = 60ms.
  // Vsync interval = 8ms.
  // Latency cap = 100ms (kPerceptibleLatencyThreshold) - 8ms - 2ms = 90ms.
  // Max present delta = 90ms - 60ms = 30ms.
  // Target present delta from presentation offset 0 = (4 + 1) * 8ms = 40ms.
  // Since max present delta (30ms) < target present delta (40ms), the target is
  // capped at 30ms. Largest deadline present delta <= 30ms is 24ms (index 1).
  // Should select index 1.
  EXPECT_EQ(1u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, frame_time,
                                       frame_time - base::Milliseconds(60)));
}

TEST_F(AndroidFrameDeadlineDeciderTest,
       LatencyCapping_FutureInputTimestamp_ClampedToZero) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "0"}});

  FrameDeadlineDecider decider(false);

  // Setup deadlines.
  // OS preferred = index 0 (present = 16ms).
  // Custom target = index 3 (present = 120ms) because we pass
  // max_pending_swaps = 14, targeting (14+1)*8ms = 120ms presentation delta.
  auto deadlines = CreatePossibleDeadlines(
      0,
      {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
       PossibleDeadline(2, base::Milliseconds(72), base::Milliseconds(80)),
       PossibleDeadline(3, base::Milliseconds(88), base::Milliseconds(96)),
       PossibleDeadline(4, base::Milliseconds(112), base::Milliseconds(120))});

  base::TimeTicks frame_time = base::TimeTicks::Now();
  // Input timestamp is 40ms in the FUTURE.
  // If clamped to 0:
  //   input_delta = 0
  //   latency_cap = 90ms (for 8ms vsync)
  //   max_present_delta = 90ms - 0 = 90ms
  //   target present delta (120ms) is capped at 90ms.
  //   Largest deadline <= 90ms is 80ms (index 1).
  //   Should select index 1.
  // If NOT clamped:
  //   input_delta = -40ms
  //   max_present_delta = 90ms - (-40ms) = 130ms
  //   target present delta (120ms) is NOT capped (120ms < 130ms).
  //   Should select index 3.
  EXPECT_EQ(1u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       15,  // max_pending_swaps = 14 -> allowed
                                            // = 15 -> target = 15 * 8 = 120ms
                                       frame_time,
                                       frame_time + base::Milliseconds(40)));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace viz
