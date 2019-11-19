// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/cancelation_signal.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/sync/base/cancelation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class BlockingTask : public CancelationObserver {
 public:
  explicit BlockingTask(CancelationSignal* cancel_signal);
  ~BlockingTask() override;

  // Starts the |exec_thread_| and uses it to execute DoRun().
  void RunAsync(base::WaitableEvent* task_start_signal,
                base::WaitableEvent* task_done_signal);

  // Blocks until canceled.  Signals |task_done_signal| when finished (either
  // via early cancel or cancel after start).  Signals |task_start_signal| if
  // and when the task starts successfully (which will not happen if the task
  // was cancelled early).
  void Run(base::WaitableEvent* task_start_signal,
           base::WaitableEvent* task_done_signal);

  // Implementation of CancelationObserver.
  // Wakes up the thread blocked in Run().
  void OnSignalReceived() override;

  // Checks if we ever did successfully start waiting for |event_|.  Be careful
  // with this.  The flag itself is thread-unsafe, and the event that flips it
  // is racy.
  bool WasStarted();

 private:
  base::WaitableEvent event_;
  base::Thread exec_thread_;
  CancelationSignal* cancel_signal_;
  bool was_started_;
};

BlockingTask::BlockingTask(CancelationSignal* cancel_signal)
    : event_(base::WaitableEvent::ResetPolicy::MANUAL,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      exec_thread_("BlockingTaskBackgroundThread"),
      cancel_signal_(cancel_signal),
      was_started_(false) {}

BlockingTask::~BlockingTask() {
  if (was_started_) {
    cancel_signal_->UnregisterHandler(this);
  }
}

void BlockingTask::RunAsync(base::WaitableEvent* task_start_signal,
                            base::WaitableEvent* task_done_signal) {
  exec_thread_.Start();
  exec_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTask::Run, base::Unretained(this),
                                base::Unretained(task_start_signal),
                                base::Unretained(task_done_signal)));
}

void BlockingTask::Run(base::WaitableEvent* task_start_signal,
                       base::WaitableEvent* task_done_signal) {
  if (cancel_signal_->TryRegisterHandler(this)) {
    DCHECK(!event_.IsSignaled());
    was_started_ = true;
    task_start_signal->Signal();
    event_.Wait();
  }
  task_done_signal->Signal();
}

void BlockingTask::OnSignalReceived() {
  event_.Signal();
}

bool BlockingTask::WasStarted() {
  return was_started_;
}

class CancelationSignalTest : public ::testing::Test {
 public:
  CancelationSignalTest();
  ~CancelationSignalTest() override;

  // Starts the blocking task on a background thread.  Does not wait for the
  // task to start.
  void StartBlockingTaskAsync();

  // Starts the blocking task on a background thread.  Does not return until
  // the task has been started.
  void StartBlockingTaskAndWaitForItToStart();

  // Cancels the blocking task.
  void CancelBlocking();

  // Verifies that the background task was canceled early.
  //
  // This method may block for a brief period of time while waiting for the
  // background thread to make progress.
  bool VerifyTaskNotStarted();

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  CancelationSignal signal_;
  base::WaitableEvent task_start_event_;
  base::WaitableEvent task_done_event_;
  BlockingTask blocking_task_;
};

CancelationSignalTest::CancelationSignalTest()
    : task_start_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
      task_done_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED),
      blocking_task_(&signal_) {}

CancelationSignalTest::~CancelationSignalTest() {}

void CancelationSignalTest::StartBlockingTaskAsync() {
  blocking_task_.RunAsync(&task_start_event_, &task_done_event_);
}

void CancelationSignalTest::StartBlockingTaskAndWaitForItToStart() {
  blocking_task_.RunAsync(&task_start_event_, &task_done_event_);
  task_start_event_.Wait();
}

void CancelationSignalTest::CancelBlocking() {
  signal_.Signal();
}

bool CancelationSignalTest::VerifyTaskNotStarted() {
  // Wait until BlockingTask::Run() has finished.
  task_done_event_.Wait();

  // Verify the background thread never started blocking.
  return !blocking_task_.WasStarted();
}

class FakeCancelationObserver : public CancelationObserver {
  void OnSignalReceived() override {}
};

TEST(CancelationSignalTest_SingleThread, CheckFlags) {
  FakeCancelationObserver observer;
  CancelationSignal signal;

  EXPECT_FALSE(signal.IsSignalled());
  signal.Signal();
  EXPECT_TRUE(signal.IsSignalled());
  EXPECT_FALSE(signal.TryRegisterHandler(&observer));
}

// Send the cancelation signal before the task is started.  This will ensure
// that the task will never be "started" (ie. TryRegisterHandler() will fail,
// so it will never start blocking on its main WaitableEvent).
TEST_F(CancelationSignalTest, CancelEarly) {
  CancelBlocking();
  StartBlockingTaskAsync();
  EXPECT_TRUE(VerifyTaskNotStarted());
}

// Send the cancelation signal after the task has started running.  This tests
// the non-early exit code path, where the task is stopped while it is in
// progress.
TEST_F(CancelationSignalTest, Cancel) {
  StartBlockingTaskAndWaitForItToStart();

  // Wait for the task to finish and let verify it has been started.
  CancelBlocking();
  EXPECT_FALSE(VerifyTaskNotStarted());
}

}  // namespace syncer
