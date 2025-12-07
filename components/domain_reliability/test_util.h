// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_TEST_UTIL_H_
#define COMPONENTS_DOMAIN_RELIABILITY_TEST_UTIL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/host_port_pair.h"
#include "url/gurl.h"

namespace net {
class IsolationInfo;
}  // namespace net

namespace domain_reliability {

// A simple test callback that remembers whether it's been called.
class TestCallback {
 public:
  TestCallback();
  ~TestCallback();

  // Returns a callback that can be called only once.
  const base::RepeatingClosure& callback() { return callback_; }
  // Returns whether the callback returned by |callback()| has been called.
  bool called() const { return called_; }

 private:
  void OnCalled();

  base::RepeatingClosure callback_;
  bool called_;
};

class MockUploader : public DomainReliabilityUploader {
 public:
  typedef base::RepeatingCallback<void(const std::string& report_json,
                                       int max_upload_depth,
                                       const GURL& upload_url,
                                       const net::IsolationInfo& isolation_info,
                                       UploadCallback upload_callback)>
      UploadRequestCallback;

  explicit MockUploader(UploadRequestCallback callback);

  ~MockUploader() override;

  virtual bool discard_uploads() const;

  // DomainReliabilityUploader implementation:
  void UploadReport(const std::string& report_json,
                    int max_upload_depth,
                    const GURL& upload_url,
                    const net::IsolationInfo& isolation_info,
                    UploadCallback callback) override;
  void Shutdown() override;
  void SetDiscardUploads(bool discard_uploads) override;
  int GetDiscardedUploadCount() const override;

 private:
  UploadRequestCallback callback_;
  bool discard_uploads_;
};

class MockTime;

class MockTickClock : public base::TickClock {
 public:
  explicit MockTickClock(MockTime* mock_time) : mock_time_(mock_time) {}
  ~MockTickClock() override = default;
  // base::TickClock implementation
  base::TimeTicks NowTicks() const override;

 private:
  raw_ptr<MockTime> mock_time_;
};

class MockTime : public MockableTime {
 public:
  MockTime();

  // N.B.: Tasks (and therefore Timers) scheduled to run in the future will
  // never be run if MockTime is destroyed before the mock time is advanced
  // to their scheduled time.
  ~MockTime() override;

  // MockableTime implementation:
  base::Time Now() const override;
  base::TimeTicks NowTicks() const override;
  std::unique_ptr<MockableTime::Timer> CreateTimer() override;
  const base::TickClock* AsTickClock() const override;

  // Pretends that |delta| has passed, and runs tasks that would've happened
  // during that interval (with |Now()| returning proper values while they
  // execute!)
  void Advance(base::TimeDelta delta);

  // Queues |task| to be run after |delay|. (Lighter-weight than mocking an
  // entire message pump.)
  void AddTask(base::TimeDelta delay, base::OnceClosure task);

 private:
  // Key used to store tasks in the task map. Includes the time the task should
  // run and a sequence number to disambiguate tasks with the same time.
  struct TaskKey {
    TaskKey(base::TimeTicks time, int sequence_number)
        : time(time),
          sequence_number(sequence_number) {}

    base::TimeTicks time;
    int sequence_number;
  };

  // Comparator for TaskKey; sorts by time, then by sequence number.
  struct TaskKeyCompare {
    bool operator() (const TaskKey& lhs, const TaskKey& rhs) const {
      return lhs.time < rhs.time ||
             (lhs.time == rhs.time &&
              lhs.sequence_number < rhs.sequence_number);
    }
  };

  typedef std::map<TaskKey, base::OnceClosure, TaskKeyCompare> TaskMap;

  void AdvanceToInternal(base::TimeTicks target_ticks);

  int elapsed_sec() { return (now_ticks_ - epoch_ticks_).InSeconds(); }

  base::Time now_;
  base::TimeTicks now_ticks_;
  base::TimeTicks epoch_ticks_;
  int task_sequence_number_;
  TaskMap tasks_;
  MockTickClock tick_clock_;
};

std::unique_ptr<DomainReliabilityConfig> MakeTestConfig();
std::unique_ptr<DomainReliabilityConfig> MakeTestConfigWithOrigin(
    const url::Origin& origin);
DomainReliabilityScheduler::Params MakeTestSchedulerParams();

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_TEST_UTIL_H_
