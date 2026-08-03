// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
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

  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(true);

  auto deadlines = CreatePossibleDeadlines(
      1, {PossibleDeadline(1, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(2, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(3, base::Milliseconds(12), base::Milliseconds(20))});

  EXPECT_EQ(1u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));
  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kPlatformPreferred, 1);
}

TEST_F(FrameDeadlineDeciderTest, SelectedSustainableDeadline_True) {
  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/true);

  // k120HzAllowedBuffers = 5, k120HzVsyncInterval = 8 ms.
  // max_sustainable_delta = (5 * 8 ms) + 1 ms = 41 ms.
  // 41 ms <= 41 ms -> sustainable (true).
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(41))});

  decider.SelectDeadline(deadlines, k120HzVsyncInterval, k120HzAllowedBuffers,
                         base::TimeTicks(), std::nullopt,
                         /*is_handling_interaction=*/false);

  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectedSustainableDeadline", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kPlatformPreferred, 1);
}

TEST_F(FrameDeadlineDeciderTest, SelectedSustainableDeadline_False) {
  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/true);

  // k120HzAllowedBuffers = 5, k120HzVsyncInterval = 8 ms.
  // max_sustainable_delta = (5 * 8 ms) + 1 ms = 41 ms.
  // 42 ms > 41 ms -> unsustainable (false).
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(42))});

  decider.SelectDeadline(deadlines, k120HzVsyncInterval, k120HzAllowedBuffers,
                         base::TimeTicks(), std::nullopt,
                         /*is_handling_interaction=*/false);

  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectedSustainableDeadline", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kPlatformPreferred, 1);
}

TEST_F(FrameDeadlineDeciderTest, OngoingInteractionSequenceRetention) {
  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start interactive sequence. Locked to 40ms delta (index
  // 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);
  histogram_tester.ExpectBucketCount(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kChromePreferredNewSequence, 1);

  // Frame 2 (t=150ms): Active interaction continues (> 50ms threshold).
  // Because both previous and current frames are interactive, sequence is
  // retained (locked to 40ms / index 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(150), std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);
  histogram_tester.ExpectBucketCount(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kOngoingSequence, 1);
}

TEST_F(FrameDeadlineDeciderTest, InteractionEndSequenceRetention) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start interactive sequence. Locked to 40ms delta (index
  // 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);

  // Frame 2 (t=20ms): Interaction ends (is_handling_interaction = false).
  // Arrives within 50ms grace period (20ms <= 50ms), so sequence is retained
  // for this frame (index 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(20), std::nullopt,
                             /*is_handling_interaction=*/false),
      2u);
}

TEST_F(FrameDeadlineDeciderTest, InteractionEndGracePeriodExpiration) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start interactive sequence. Locked to 40ms delta (index
  // 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);

  // Frame 2 (t=60ms): Interaction ends (is_handling_interaction = false) after
  // 60ms gap (> 50ms threshold). Because interaction ended,
  // max_non_interactive_idle_duration_ (50ms) applies. 60ms > 50ms, so sequence
  // resets (recalculates for max_allowed_buffers = 3 -> 24ms / index 1).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(60), std::nullopt,
                             /*is_handling_interaction=*/false),
      1u);
}

TEST_F(FrameDeadlineDeciderTest, NonInteractiveToInteractiveTransitionTimeout) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start non-interactive sequence. Locked to 40ms delta
  // (index 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/false),
      2u);

  // Frame 2 (t=80ms): Interactive gesture starts (is_handling_interaction =
  // true) after 80ms gap. Because previous frame was non-interactive, the
  // transition frame uses max_non_interactive_idle_duration_ (50ms). 80ms >
  // 50ms, so preceding sequence resets and target deadline is recalculated for
  // max_allowed_buffers = 3 (24ms / index 1).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(80), std::nullopt,
                             /*is_handling_interaction=*/true),
      1u);
}

TEST_F(FrameDeadlineDeciderTest,
       NotifyMinSupportedVsyncIntervalCapsPresentationDelta) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);

  auto deadlines_60hz = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(32)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(48)),
          PossibleDeadline(4, base::Milliseconds(48), base::Milliseconds(64))});

  // 1. Without NotifyMinSupportedVsyncInterval, target_present_delta (48ms)
  // selects Index 2 (48ms).
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            2u);

  decider.OnDisplayInvisible();

  // 2. After NotifyMinSupportedVsyncInterval(8ms) [120 Hz peak -> presentation
  // cap = 24ms], target_present_delta is capped to <= 24ms, selecting Index 0
  // (16ms) instead of Index 2.
  decider.NotifyMinSupportedVsyncInterval(base::Milliseconds(8));
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            0u);
}

TEST_F(FrameDeadlineDeciderTest,
       NotifyMinSupportedVsyncIntervalUpdatedDuringSession) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);

  auto deadlines_60hz = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(32)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(48)),
          PossibleDeadline(4, base::Milliseconds(48), base::Milliseconds(64))});

  // 1. Notify 16ms min supported vsync interval (60 Hz only display ->
  // presentation cap = 48ms). Target present delta (48ms) <= presentation cap
  // (48ms), so Index 2 (48ms) is selected.
  decider.NotifyMinSupportedVsyncInterval(base::Milliseconds(16));
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            2u);

  decider.OnDisplayInvisible();

  // 2. Display capabilities change (e.g., external 120 Hz display connected,
  // 8ms min vsync interval). Presentation cap is now 24ms. Target present delta
  // is capped to <= 24ms, selecting Index 0 (16ms).
  decider.NotifyMinSupportedVsyncInterval(base::Milliseconds(8));
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            0u);
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
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));
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
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SanityGuardFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-4"}});

  base::HistogramTester histogram_tester;
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
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));
  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kOsPreferredChromePreferredSooner,
      1);
}

TEST_F(AndroidFrameDeadlineDeciderTest, SelectionReason_FallbackExceedsTarget) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "-4"}});

  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(false);

  // Setup 120Hz deadlines.
  // num_buffers = 4 + 1 = 5.
  // Target present multiplier = max(1, 5 - 4) = 1.
  // Target present delta = 1 * 8ms = 8ms.
  // Preferred index = 0 (16ms).
  // Custom target is index 0 (present = 16ms), which is > 8ms target.
  // Falls back to OS preferred with kOsPreferredNoDeadlineWithinTarget.
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16))});

  EXPECT_EQ(0u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));

  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kOsPreferredNoDeadlineWithinTarget,
      1);
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
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));
}

TEST_F(AndroidFrameDeadlineDeciderTest, SequenceLockingAndReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kUseAndroidCustomFrameDeadlines,
      {{"presentation_offset", "0"}});

  FrameDeadlineDecider decider(false);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

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
  EXPECT_EQ(
      1u, decider.SelectDeadline(deadlines_1, k120HzVsyncInterval,
                                 k120HzAllowedBuffers, base_time, std::nullopt,
                                 /*is_handling_interaction=*/false));

  // 2. Subsequent frame 30ms later: max_pending_swaps = 2.
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
                                       base_time + base::Milliseconds(30),
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));

  // 3. New frame 90ms after start (60ms gap > 50ms timeout).
  // Target = 24ms. Deadlines: [16ms (pref), 24ms, 40ms]
  // Should recalculate and select index 1 (24ms).
  EXPECT_EQ(1u, decider.SelectDeadline(deadlines_2, k120HzVsyncInterval, 3,
                                       base_time + base::Milliseconds(90),
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));
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
                                       frame_time - base::Milliseconds(10),
                                       /*is_handling_interaction=*/false));
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
                                       frame_time - base::Milliseconds(60),
                                       /*is_handling_interaction=*/false));
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
                                       frame_time + base::Milliseconds(40),
                                       /*is_handling_interaction=*/false));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace viz
