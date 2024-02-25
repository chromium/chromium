// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/unnecessary_discard_monitor.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

class UnnecessaryDiscardMonitorTest : public testing::Test {
 public:
  UnnecessaryDiscardMonitorTest() = default;
  ~UnnecessaryDiscardMonitorTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  UnnecessaryDiscardMonitor monitor_;
  base::HistogramTester histogram_tester_;
};

TEST_F(UnnecessaryDiscardMonitorTest, TestNoEventsReportsNothing) {
  // Reset the monitor to force CalculateAndReport().
  monitor_.OnReclaimTargetEnd();
  histogram_tester_.ExpectTotalCount("Discarding.DiscardsDrivenByStaleSignal",
                                     0);
}

TEST_F(UnnecessaryDiscardMonitorTest, TestReclaimTargetAgeIsReported) {
  base::TimeTicks reclaim_target_origin = base::TimeTicks::Now();

  base::TimeDelta target_age = base::Seconds(5);

  task_environment_.FastForwardBy(target_age);

  // Start processing a reclaim target that was calculated some time ago.
  monitor_.OnReclaimTargetBegin({100, reclaim_target_origin});

  histogram_tester_.ExpectTimeBucketCount("Discarding.ReclaimTargetAge",
                                          target_age, 1);
}

TEST_F(UnnecessaryDiscardMonitorTest, TestNoUnnecessaryDiscardsIsReported) {
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});
  monitor_.OnDiscard(100, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  histogram_tester_.ExpectUniqueSample("Discarding.DiscardsDrivenByStaleSignal",
                                       0, 1);
}

TEST_F(UnnecessaryDiscardMonitorTest, TestSingleUnnecessaryDiscardIsReported) {
  // Start processing a reclaim target that was calculated now.
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});

  // Store the time for the next reclaim event.
  base::TimeTicks next_reclaim_event_time =
      base::TimeTicks::Now() + base::Milliseconds(1000);

  task_environment_.FastForwardBy(base::Milliseconds(1001));
  monitor_.OnDiscard(100, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnReclaimTargetBegin({100, next_reclaim_event_time});
  task_environment_.FastForwardBy(base::Milliseconds(10));
  monitor_.OnDiscard(100, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  // The first reclaim event should have no unnecessary discards, but the second
  // one should have a single unnecessary discard.
  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      0, 1);
  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      1, 1);
}

TEST_F(UnnecessaryDiscardMonitorTest,
       TestMultipleUnnecessaryDiscardsAreReported) {
  // Start processing a reclaim target that was calculated now.
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});

  // Store the time for the next reclaim event.
  base::TimeTicks next_reclaim_event_time =
      base::TimeTicks::Now() + base::Milliseconds(1000);

  // Discard a tab from this reclaim event.
  task_environment_.FastForwardBy(base::Milliseconds(1001));
  monitor_.OnDiscard(100, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  // Start processing a reclaim target that was created before the first tab was
  // discarded. Since this reclaim target is the same as the previous reclaim
  // target, all kills from this reclaim target should be unnecessary.
  monitor_.OnReclaimTargetBegin({100, next_reclaim_event_time});
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(500, base::TimeTicks::Now());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(500, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  // The first reclaim event should have no unnecessary discards, but the second
  // one should have two unnecessary discards.
  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      0, 1);
  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      2, 1);
}

TEST_F(UnnecessaryDiscardMonitorTest,
       TestIncreasingReclaimTargetIsNotReportedAsUnnecessary) {
  // Start processing a reclaim target that was calculated now.
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});

  // Store the time for the next reclaim event.
  base::TimeTicks next_reclaim_event_time =
      base::TimeTicks::Now() + base::Milliseconds(1000);

  // Discard a tab from this reclaim event before the next reclaim event's
  // origin.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  monitor_.OnDiscard(80, base::TimeTicks::Now());
  // Discard another tab from this reclaim event after the next reclaim event's
  // origin.
  task_environment_.FastForwardBy(base::Milliseconds(501));
  monitor_.OnDiscard(30, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  // In total, the first reclaim event discarded 110 when the target was 100. 30
  // of that discard was discarded after next_reclaim_event_time which means
  // that there will be no unnecessary discards if the next reclaim event only
  // discards once.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnReclaimTargetBegin({110, next_reclaim_event_time});
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(120, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      0, 2);
}

TEST_F(UnnecessaryDiscardMonitorTest,
       TestIncreasingReclaimTargetWithUnnecessaryDiscard) {
  // Start processing a reclaim target that was calculated now.
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});

  // Store the time for the next reclaim event.
  base::TimeTicks next_reclaim_event_time =
      base::TimeTicks::Now() + base::Milliseconds(1000);

  // Discard a tab from this reclaim event before the next reclaim event's
  // origin.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  monitor_.OnDiscard(80, base::TimeTicks::Now());
  // Discard another tab from this reclaim event after the next reclaim event's
  // origin.
  task_environment_.FastForwardBy(base::Milliseconds(501));
  monitor_.OnDiscard(30, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  // In total, the first reclaim event discarded 110 when the target was 100. 30
  // of that discard was discarded after next_reclaim_event_time which means
  // that one discard of size 80 is necessary, but an additional discard of size
  // 40 is unnecessary.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnReclaimTargetBegin({110, next_reclaim_event_time});
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(80, base::TimeTicks::Now());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(40, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      0, 1);
  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      1, 1);
}

TEST_F(UnnecessaryDiscardMonitorTest,
       TestEveryReclaimEventReportsZeroUnnecessaryKills) {
  // The normal sequence that should have no unnecessary kills.
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(150, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  task_environment_.FastForwardBy(base::Milliseconds(1000));
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(150, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  // Check that both reclaim events had no unnecessary discards
  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      0, 2);
}

TEST_F(UnnecessaryDiscardMonitorTest, TestTwoPreceedingReclaimTargets) {
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});
  task_environment_.FastForwardBy(base::Milliseconds(1));
  monitor_.OnDiscard(150, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  task_environment_.FastForwardBy(base::Milliseconds(1000));
  monitor_.OnReclaimTargetBegin({100, base::TimeTicks::Now()});
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  monitor_.OnDiscard(150, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  // Process a reclaim target that was created before the most recent discard.
  monitor_.OnReclaimTargetBegin(
      {100, base::TimeTicks::Now() - base::Milliseconds(500)});
  // This discard is unnecessary since the previous target was the same and
  // already discarded.
  monitor_.OnDiscard(200, base::TimeTicks::Now());
  monitor_.OnReclaimTargetEnd();

  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      0, 2);
  histogram_tester_.ExpectBucketCount("Discarding.DiscardsDrivenByStaleSignal",
                                      1, 1);
}

}  // namespace memory_pressure
