// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_listener_renderer_impl.h"
#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker_impl.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_io_thread.h"
#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using UnblockAction = BoundSessionRequestThrottledListener::UnblockAction;
using ResumeOrCancelThrottledRequestCallback =
    BoundSessionRequestThrottledListener::
        ResumeOrCancelThrottledRequestCallback;

using ::testing::_;
}  // namespace

class MockBoundSessionRequestThrottledInRendererManager
    : public BoundSessionRequestThrottledInRendererManager {
 public:
  MockBoundSessionRequestThrottledInRendererManager() {
    sequence_checker_.DetachFromSequence();
    ON_CALL(*this, OnRequestBlockedOnCookie(_))
        .WillByDefault(testing::Invoke(
            this, &MockBoundSessionRequestThrottledInRendererManager::
                      OnRequestBlockedOnCookieCalled));
  }

  MOCK_METHOD(void,
              OnRequestBlockedOnCookie,
              (BoundSessionRequestThrottledListener::
                   ResumeOrCancelThrottledRequestCallback callback),
              (override));

  void BindSequenceChecker() {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
  }

 private:
  ~MockBoundSessionRequestThrottledInRendererManager() override = default;

  void OnRequestBlockedOnCookieCalled(
      ResumeOrCancelThrottledRequestCallback callback) {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
    std::move(callback).Run(UnblockAction::kResume);
  }

  // Used to verify `OnRequestBlockedOnCookie()` is called on the right
  // sequence.
  base::SequenceCheckerImpl sequence_checker_;
};

TEST(BoundSessionRequestThrottledListenerRendererImplTest,
     OnRequestBlockedOnCookie) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS};
  scoped_refptr<MockBoundSessionRequestThrottledInRendererManager> manager =
      base::MakeRefCounted<MockBoundSessionRequestThrottledInRendererManager>();
  base::TestIOThread io_thread(base::TestIOThread::kAutoStart);
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      io_thread.task_runner();

  // Initialize the mock to ensure the sequence checker is attached to the
  // `io_task_runner`. After initialization,
  // `MockManager::OnRequestBlockedOnCookie` will always verify, it is called on
  // the `io_task_runner`.
  base::RunLoop initialize_mock_run_loop;
  io_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&MockBoundSessionRequestThrottledInRendererManager::
                         BindSequenceChecker,
                     manager),
      base::BindOnce(
          [](base::OnceClosure callback) { std::move(callback).Run(); },
          initialize_mock_run_loop.QuitClosure()));
  initialize_mock_run_loop.Run();

  EXPECT_CALL(*manager, OnRequestBlockedOnCookie(_));

  // Used to check that the callback passed to
  // `BoundSessionRequestThrottledListenerRendererImpl` is executed on the
  // caller's sequence runner.
  base::SequenceCheckerImpl sequence_checker;
  BoundSessionRequestThrottledListenerRendererImpl listener(manager,
                                                            io_task_runner);
  base::RunLoop run_loop;
  listener.OnRequestBlockedOnCookie(base::BindOnce(
      [](base::SequenceCheckerImpl checker, base::OnceClosure callback,
         UnblockAction action) {
        EXPECT_TRUE(checker.CalledOnValidSequence());
        std::move(callback).Run();
      },
      std::move(sequence_checker), run_loop.QuitClosure()));

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(manager.get());
}
