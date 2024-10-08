// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/test_util.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/domain_reliability/scheduler.h"
#include "net/base/isolation_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

class MockTimer : public MockableTime::Timer {
 public:
  MockTimer(MockTime* time)
      : time_(time), running_(false), callback_sequence_number_(0) {
    DCHECK(time);
  }

  ~MockTimer() override = default;

  // MockableTime::Timer implementation:
  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::OnceClosure user_task) override {
    DCHECK(user_task);

    if (running_)
      ++callback_sequence_number_;
    running_ = true;
    user_task_ = std::move(user_task);
    time_->AddTask(delay, base::BindOnce(&MockTimer::OnDelayPassed,
                                         weak_factory_.GetWeakPtr(),
                                         callback_sequence_number_));
  }

  void Stop() override {
    if (running_) {
      ++callback_sequence_number_;
      running_ = false;
    }
  }

  bool IsRunning() override { return running_; }

 private:
  void OnDelayPassed(int expected_callback_sequence_number) {
    if (callback_sequence_number_ != expected_callback_sequence_number)
      return;

    DCHECK(running_);
    running_ = false;

    // Grab user task in case it re-entrantly starts the timer again.
    base::OnceClosure task_to_run = std::move(user_task_);
    user_task_.Reset();
    std::move(task_to_run).Run();
  }

  raw_ptr<MockTime> time_;
  bool running_;
  int callback_sequence_number_;
  base::OnceClosure user_task_;
  base::WeakPtrFactory<MockTimer> weak_factory_{this};
};

}  // namespace

TestCallback::TestCallback()
    : callback_(
          base::BindRepeating(&TestCallback::OnCalled, base::Unretained(this))),
      called_(false) {}

TestCallback::~TestCallback() = default;

void TestCallback::OnCalled() {
  EXPECT_FALSE(called_);
  called_ = true;
}

MockUploader::MockUploader(UploadRequestCallback callback)
    : callback_(callback), discard_uploads_(true) {}

MockUploader::~MockUploader() = default;

bool MockUploader::discard_uploads() const { return discard_uploads_; }

void MockUploader::UploadReport(const std::string& report_json,
                                int max_upload_depth,
                                const GURL& upload_url,
                                const net::IsolationInfo& isolation_info,
                                UploadCallback callback) {
  callback_.Run(report_json, max_upload_depth, upload_url, isolation_info,
                std::move(callback));
}

void MockUploader::Shutdown() {}

void MockUploader::SetDiscardUploads(bool discard_uploads) {
  discard_uploads_ = discard_uploads;
}

int MockUploader::GetDiscardedUploadCount() const {
  return 0;
}

base::TimeTicks MockTickClock::NowTicks() const {
  return mock_time_->NowTicks();
}

MockTime::MockTime()
    : now_(base::Time::Now()),
      now_ticks_(base::TimeTicks::Now()),
      epoch_ticks_(now_ticks_),
      task_sequence_number_(0),
      tick_clock_(this) {
  VLOG(1) << "Creating mock time: T=" << elapsed_sec() << "s";
}

MockTime::~MockTime() = default;

base::Time MockTime::Now() const {
  return now_;
}
base::TimeTicks MockTime::NowTicks() const {
  return now_ticks_;
}

std::unique_ptr<MockableTime::Timer> MockTime::CreateTimer() {
  return std::unique_ptr<MockableTime::Timer>(new MockTimer(this));
}

const base::TickClock* MockTime::AsTickClock() const {
  return &tick_clock_;
}

void MockTime::Advance(base::TimeDelta delta) {
  base::TimeTicks target_ticks = now_ticks_ + delta;

  while (!tasks_.empty() && tasks_.begin()->first.time <= target_ticks) {
    TaskKey key = tasks_.begin()->first;
    base::OnceClosure task = std::move(tasks_.begin()->second);
    tasks_.erase(tasks_.begin());

    DCHECK(now_ticks_ <= key.time);
    DCHECK(key.time <= target_ticks);
    AdvanceToInternal(key.time);
    VLOG(1) << "Advancing mock time: task at T=" << elapsed_sec() << "s";

    std::move(task).Run();
  }

  DCHECK(now_ticks_ <= target_ticks);
  AdvanceToInternal(target_ticks);
  VLOG(1) << "Advanced mock time: T=" << elapsed_sec() << "s";
}

void MockTime::AddTask(base::TimeDelta delay, base::OnceClosure task) {
  tasks_[TaskKey(now_ticks_ + delay, task_sequence_number_++)] =
      std::move(task);
}

void MockTime::AdvanceToInternal(base::TimeTicks target_ticks) {
  base::TimeDelta delta = target_ticks - now_ticks_;
  now_ += delta;
  now_ticks_ += delta;
}

DomainReliabilityScheduler::Params MakeTestSchedulerParams() {
  DomainReliabilityScheduler::Params params;
  params.minimum_upload_delay = base::Minutes(1);
  params.maximum_upload_delay = base::Minutes(5);
  params.upload_retry_interval = base::Seconds(15);
  return params;
}

std::unique_ptr<DomainReliabilityConfig> MakeTestConfig() {
  return MakeTestConfigWithOrigin(
      url::Origin::Create(GURL("https://example/")));
}

std::unique_ptr<DomainReliabilityConfig> MakeTestConfigWithOrigin(
    const url::Origin& origin) {
  DomainReliabilityConfig* config = new DomainReliabilityConfig();
  config->origin = origin;
  config->collectors.push_back(
      std::make_unique<GURL>("https://exampleuploader/upload"));
  config->failure_sample_rate = 1.0;
  config->success_sample_rate = 0.0;

  DCHECK(config->IsValid());

  return std::unique_ptr<DomainReliabilityConfig>(config);
}

}  // namespace domain_reliability
