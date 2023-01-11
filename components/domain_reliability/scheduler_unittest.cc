// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/scheduler.h"

#include <stddef.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/test_util.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

using base::TimeTicks;

class DomainReliabilitySchedulerTest : public testing::Test {
 public:
  DomainReliabilitySchedulerTest()
      : num_collectors_(0),
        params_(MakeTestSchedulerParams()),
        callback_called_(false) {}

  void CreateScheduler(int num_collectors) {
    DCHECK_LT(0, num_collectors);
    DCHECK(!scheduler_);

    num_collectors_ = num_collectors;
    scheduler_ = std::make_unique<DomainReliabilityScheduler>(
        &time_, num_collectors_, params_,
        base::BindRepeating(
            &DomainReliabilitySchedulerTest::ScheduleUploadCallback,
            base::Unretained(this)));
    scheduler_->MakeDeterministicForTesting();
  }

  void NotifySuccessfulUpload() {
    DomainReliabilityUploader::UploadResult result;
    result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
    scheduler_->OnUploadComplete(result);
  }

  void NotifyFailedUpload() {
    DomainReliabilityUploader::UploadResult result;
    result.status = DomainReliabilityUploader::UploadResult::FAILURE;
    scheduler_->OnUploadComplete(result);
  }

  void NotifyRetryAfterUpload(base::TimeDelta retry_after) {
    DomainReliabilityUploader::UploadResult result;
    result.status = DomainReliabilityUploader::UploadResult::RETRY_AFTER;
    result.retry_after = retry_after;
    scheduler_->OnUploadComplete(result);
  }

  ::testing::AssertionResult CheckNoPendingUpload() {
    DCHECK(scheduler_);

    if (!callback_called_)
      return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
           << "expected no upload, got upload between "
           << callback_min_.InSeconds() << " and "
           << callback_max_.InSeconds() << " seconds from now";
  }

  ::testing::AssertionResult CheckPendingUpload(base::TimeDelta expected_min,
                                                base::TimeDelta expected_max) {
    DCHECK(scheduler_);
    DCHECK_LE(expected_min.InMicroseconds(), expected_max.InMicroseconds());

    if (callback_called_ && expected_min == callback_min_
                         && expected_max == callback_max_) {
      callback_called_ = false;
      return ::testing::AssertionSuccess();
    }

    if (callback_called_) {
      return ::testing::AssertionFailure()
             << "expected upload between " << expected_min.InSeconds()
             << " and " << expected_max.InSeconds() << " seconds from now, "
             << "got upload between " << callback_min_.InSeconds()
             << " and " << callback_max_.InSeconds() << " seconds from now";
    } else {
      return ::testing::AssertionFailure()
             << "expected upload between " << expected_min.InSeconds()
             << " and " << expected_max.InSeconds() << " seconds from now, "
             << "got no upload";
    }
  }

  ::testing::AssertionResult CheckStartingUpload(size_t expected_collector) {
    DCHECK(scheduler_);
    DCHECK_GT(num_collectors_, expected_collector);

    size_t collector = scheduler_->OnUploadStart();
    if (collector == expected_collector)
      return ::testing::AssertionSuccess();

    return ::testing::AssertionFailure()
           << "expected upload to collector " << expected_collector
           << ", got upload to collector " << collector;
  }

  base::TimeDelta min_delay() const { return params_.minimum_upload_delay; }
  base::TimeDelta max_delay() const { return params_.maximum_upload_delay; }
  base::TimeDelta retry_interval() const {
    return params_.upload_retry_interval;
  }
  base::TimeDelta zero_delta() const { return base::Microseconds(0); }

 protected:
  void ScheduleUploadCallback(base::TimeDelta min, base::TimeDelta max) {
    callback_called_ = true;
    callback_min_ = min;
    callback_max_ = max;
  }

  MockTime time_;
  size_t num_collectors_;
  DomainReliabilityScheduler::Params params_;
  std::unique_ptr<DomainReliabilityScheduler> scheduler_;

  bool callback_called_;
  base::TimeDelta callback_min_;
  base::TimeDelta callback_max_;
};

TEST_F(DomainReliabilitySchedulerTest, Create) {
  CreateScheduler(1);
}

TEST_F(DomainReliabilitySchedulerTest, UploadNotPendingWithoutBeacon) {
  CreateScheduler(1);

  ASSERT_TRUE(CheckNoPendingUpload());
}

TEST_F(DomainReliabilitySchedulerTest, SuccessfulUploads) {
  CreateScheduler(1);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifySuccessfulUpload();

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifySuccessfulUpload();
}

TEST_F(DomainReliabilitySchedulerTest, RetryAfter) {
  CreateScheduler(1);

  base::TimeDelta retry_after_interval = base::Minutes(30);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifyRetryAfterUpload(retry_after_interval);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(retry_after_interval, retry_after_interval));
  time_.Advance(retry_after_interval);
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifySuccessfulUpload();
}

TEST_F(DomainReliabilitySchedulerTest, Failover) {
  CreateScheduler(2);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifyFailedUpload();

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(zero_delta(), max_delay() - min_delay()));
  // Don't need to advance; should retry immediately.
  ASSERT_TRUE(CheckStartingUpload(1));
  NotifySuccessfulUpload();
}

TEST_F(DomainReliabilitySchedulerTest, FailedAllCollectors) {
  CreateScheduler(2);

  // T = 0
  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());

  // T = min_delay
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifyFailedUpload();

  ASSERT_TRUE(CheckPendingUpload(zero_delta(), max_delay() - min_delay()));
  // Don't need to advance; should retry immediately.
  ASSERT_TRUE(CheckStartingUpload(1));
  NotifyFailedUpload();

  ASSERT_TRUE(CheckPendingUpload(retry_interval(), max_delay() - min_delay()));
  time_.Advance(retry_interval());

  // T = min_delay + retry_interval
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifyFailedUpload();

  ASSERT_TRUE(CheckPendingUpload(
      zero_delta(),
      max_delay() - min_delay() - retry_interval()));
  ASSERT_TRUE(CheckStartingUpload(1));
  NotifyFailedUpload();
}

// Make sure that the scheduler uses the first available collector at upload
// time, even if it wasn't available at scheduling time.
TEST_F(DomainReliabilitySchedulerTest, DetermineCollectorAtUpload) {
  CreateScheduler(2);

  // T = 0
  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());

  // T = min_delay
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifyFailedUpload();

  ASSERT_TRUE(CheckPendingUpload(zero_delta(), max_delay() - min_delay()));
  time_.Advance(retry_interval());

  // T = min_delay + retry_interval; collector 0 should be active again.
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifySuccessfulUpload();
}

TEST_F(DomainReliabilitySchedulerTest, BeaconWhilePending) {
  CreateScheduler(1);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));

  // Second beacon should not call callback again.
  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckNoPendingUpload());
  time_.Advance(min_delay());

  // No pending upload after beacon.
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifySuccessfulUpload();
  ASSERT_TRUE(CheckNoPendingUpload());
}

TEST_F(DomainReliabilitySchedulerTest, BeaconWhileUploading) {
  CreateScheduler(1);

  scheduler_->OnBeaconAdded();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));
  time_.Advance(min_delay());

  // If a beacon arrives during the upload, a new upload should be pending.
  ASSERT_TRUE(CheckStartingUpload(0));
  scheduler_->OnBeaconAdded();
  NotifySuccessfulUpload();
  ASSERT_TRUE(CheckPendingUpload(min_delay(), max_delay()));

  time_.Advance(min_delay());
  ASSERT_TRUE(CheckStartingUpload(0));
  NotifySuccessfulUpload();
  ASSERT_TRUE(CheckNoPendingUpload());
}

}  // namespace
}  // namespace domain_reliability
