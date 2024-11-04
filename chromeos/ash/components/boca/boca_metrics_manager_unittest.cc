// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_metrics_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace ash::boca {

constexpr base::TimeDelta fast_forward_timeskip =
    BocaSessionManager::kIndefinitePollingInterval + base::Seconds(1);
constexpr char kTestUrl1[] = "https://www.test1.com";
constexpr char kTestUrl2[] = "https://www.test2.com";

class BocaMetricsManagerTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

class BocaMetricsManagerProducerTest : public BocaMetricsManagerTest {
 protected:
  BocaMetricsManager metrics_manager_{/*is_producer*/ true};
};

TEST_F(BocaMetricsManagerProducerTest,
       RecordLockedAndUnlockedStateDurationPercentageMetricsCorrectly) {
  base::HistogramTester histograms;

  metrics_manager_.OnSessionStarted("test_session_id", ::boca::UserIdentity());
  ::boca::Bundle bundle_1;
  bundle_1.set_locked(true);

  metrics_manager_.OnBundleUpdated(bundle_1);
  task_environment_.FastForwardBy(fast_forward_timeskip);

  ::boca::Bundle bundle_2;
  bundle_2.set_locked(false);
  metrics_manager_.OnBundleUpdated(bundle_2);
  task_environment_.FastForwardBy(2 * fast_forward_timeskip);

  metrics_manager_.OnSessionEnded("test_session_id");
  const base::TimeDelta total_duration = 3 * fast_forward_timeskip;

  // We waited for one `fast_forward_timeskip` duration before we got the update
  // to unlock so the percentage for locked mode is 1 `fast_forward_timeskip`.
  // Similarly, we waited two `fast_forward_timeskip` after the update and for
  // session end so the percentage for unlocked mode is 2 *
  // `fast_forward_timeskip`.
  const double expected_percentage_locked =
      100.0 * (fast_forward_timeskip / total_duration);
  const double expected_percentage_unlocked =
      100.0 - expected_percentage_locked;
  histograms.ExpectTotalCount(kBocaOnTaskLockedSessionDurationPercentage, 1);
  histograms.ExpectBucketCount(kBocaOnTaskLockedSessionDurationPercentage,
                               expected_percentage_locked, 1);
  histograms.ExpectTotalCount(kBocaOnTaskUnlockedSessionDurationPercentage, 1);
  histograms.ExpectBucketCount(kBocaOnTaskUnlockedSessionDurationPercentage,
                               expected_percentage_unlocked, 1);
}

TEST_F(BocaMetricsManagerProducerTest,
       RecordNumOfTabsWhenSessionEndedMetricsForProducerCorrectly) {
  base::HistogramTester histograms;

  metrics_manager_.OnSessionStarted("test_session_id", ::boca::UserIdentity());
  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  bundle_1.add_content_configs()->set_url(kTestUrl2);
  metrics_manager_.OnBundleUpdated(bundle_1);
  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl1);
  metrics_manager_.OnBundleUpdated(bundle_2);
  metrics_manager_.OnSessionEnded("test_session_id");

  const int expected_num_of_tabs = 1;
  histograms.ExpectTotalCount(kBocaOnTaskNumOfTabsWhenSessionEnded, 1);
  histograms.ExpectBucketCount(kBocaOnTaskNumOfTabsWhenSessionEnded,
                               expected_num_of_tabs, 1);
}

TEST_F(BocaMetricsManagerProducerTest,
       RecordMaxNumOfTabsDuringSessionMetricsForProducerCorrectly) {
  base::HistogramTester histograms;

  metrics_manager_.OnSessionStarted("test_session_id", ::boca::UserIdentity());
  ::boca::Bundle bundle_1;
  bundle_1.add_content_configs()->set_url(kTestUrl1);
  bundle_1.add_content_configs()->set_url(kTestUrl2);
  metrics_manager_.OnBundleUpdated(bundle_1);
  ::boca::Bundle bundle_2;
  bundle_2.add_content_configs()->set_url(kTestUrl1);
  metrics_manager_.OnBundleUpdated(bundle_2);
  metrics_manager_.OnSessionEnded("test_session_id");

  const int expected_max_num_of_tabs = 2;
  histograms.ExpectTotalCount(kBocaOnTaskMaxNumOfTabsDuringSession, 1);
  histograms.ExpectBucketCount(kBocaOnTaskMaxNumOfTabsDuringSession,
                               expected_max_num_of_tabs, 1);
}

class BocaMetricsManagerConsumerTest : public BocaMetricsManagerTest {
 protected:
  BocaMetricsManager metrics_manager_{/*is_producer*/ false};
};

TEST_F(BocaMetricsManagerConsumerTest,
       DoNotRecordLockedAndUnlockedStateDurationPercentageMetricsForConsumer) {
  base::HistogramTester histograms;

  metrics_manager_.OnSessionStarted("test_session_id", ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.set_locked(true);
  metrics_manager_.OnBundleUpdated(bundle);
  task_environment_.FastForwardBy(fast_forward_timeskip);
  metrics_manager_.OnSessionEnded("test_session_id");

  histograms.ExpectTotalCount(kBocaOnTaskLockedSessionDurationPercentage, 0);
  histograms.ExpectTotalCount(kBocaOnTaskUnlockedSessionDurationPercentage, 0);
}

TEST_F(BocaMetricsManagerConsumerTest,
       DoNotRecordNumOfTabsWhenSessionEndedMetricsForConsumer) {
  base::HistogramTester histograms;

  metrics_manager_.OnSessionStarted("test_session_id", ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  metrics_manager_.OnBundleUpdated(bundle);
  metrics_manager_.OnSessionEnded("test_session_id");

  histograms.ExpectTotalCount(kBocaOnTaskNumOfTabsWhenSessionEnded, 0);
}

TEST_F(BocaMetricsManagerConsumerTest,
       DoNotRecordMaxNumOfTabsDuringSessionMetricsForConsumer) {
  base::HistogramTester histograms;

  metrics_manager_.OnSessionStarted("test_session_id", ::boca::UserIdentity());
  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  metrics_manager_.OnBundleUpdated(bundle);
  metrics_manager_.OnSessionEnded("test_session_id");

  histograms.ExpectTotalCount(kBocaOnTaskMaxNumOfTabsDuringSession, 0);
}
}  // namespace ash::boca
