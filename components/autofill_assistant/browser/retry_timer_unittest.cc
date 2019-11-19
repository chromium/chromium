// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/retry_timer.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace autofill_assistant {

namespace {

MATCHER_P(EqualsStatus, status, "") {
  return arg.proto_status() == status.proto_status();
}

class RetryTimerTest : public testing::Test {
 protected:
  RetryTimerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void FastForwardOneSecond() {
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  base::RepeatingCallback<void(base::OnceCallback<void(const ClientStatus&)>)>
  AlwaysFailsCallback() {
    return base::BindRepeating(&RetryTimerTest::AlwaysFails,
                               base::Unretained(this));
  }

  void AlwaysFails(base::OnceCallback<void(const ClientStatus&)> callback) {
    try_count_++;
    std::move(callback).Run(ClientStatus());
  }

  base::RepeatingCallback<void(base::OnceCallback<void(const ClientStatus&)>)>
  SucceedsOnceCallback(int succeds_at) {
    return base::BindRepeating(&RetryTimerTest::SucceedsOnce,
                               base::Unretained(this), succeds_at);
  }

  void SucceedsOnce(int succeeds_at,
                    base::OnceCallback<void(const ClientStatus&)> callback) {
    EXPECT_GE(succeeds_at, try_count_);
    bool success = succeeds_at == try_count_;
    try_count_++;
    std::move(callback).Run(success ? OkClientStatus() : ClientStatus());
  }

  base::RepeatingCallback<void(base::OnceCallback<void(const ClientStatus&)>)>
  CaptureCallback() {
    return base::BindRepeating(&RetryTimerTest::Capture,
                               base::Unretained(this));
  }

  void Capture(base::OnceCallback<void(const ClientStatus&)> callback) {
    try_count_++;
    captured_callback_ = std::move(callback);
  }

  // task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_environment_;

  int try_count_ = 0;
  base::OnceCallback<void(const ClientStatus&)> captured_callback_;
  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>>
      done_callback_;
};

TEST_F(RetryTimerTest, TryOnceAndSucceed) {
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(done_callback_, Run(EqualsStatus(OkClientStatus())));
  retry_timer.Start(base::TimeDelta::FromSeconds(10), SucceedsOnceCallback(0),
                    done_callback_.Get());
  EXPECT_EQ(1, try_count_);
}

TEST_F(RetryTimerTest, TryOnceAndFail) {
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  EXPECT_CALL(done_callback_, Run(EqualsStatus(ClientStatus())));
  retry_timer.Start(base::TimeDelta::FromSeconds(0), AlwaysFailsCallback(),
                    done_callback_.Get());
  EXPECT_EQ(1, try_count_);
}

TEST_F(RetryTimerTest, TryMultipleTimesAndSucceed) {
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  retry_timer.Start(base::TimeDelta::FromSeconds(10), SucceedsOnceCallback(2),
                    done_callback_.Get());
  EXPECT_EQ(1, try_count_);
  FastForwardOneSecond();
  EXPECT_EQ(2, try_count_);
  EXPECT_CALL(done_callback_, Run(EqualsStatus(OkClientStatus())));
  FastForwardOneSecond();
  EXPECT_EQ(3, try_count_);
}

TEST_F(RetryTimerTest, TryMultipleTimesAndFail) {
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  retry_timer.Start(base::TimeDelta::FromSeconds(2), AlwaysFailsCallback(),
                    done_callback_.Get());
  EXPECT_EQ(1, try_count_);
  FastForwardOneSecond();
  EXPECT_EQ(2, try_count_);
  EXPECT_CALL(done_callback_, Run(EqualsStatus(ClientStatus())));
  FastForwardOneSecond();
  EXPECT_EQ(3, try_count_);
}

TEST_F(RetryTimerTest, Cancel) {
  EXPECT_CALL(done_callback_, Run(_)).Times(0);
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  retry_timer.Start(base::TimeDelta::FromSeconds(10), AlwaysFailsCallback(),
                    done_callback_.Get());
  EXPECT_EQ(1, try_count_);
  retry_timer.Cancel();
  FastForwardOneSecond();  // nothing should happen
}

TEST_F(RetryTimerTest, CancelWithPendingCallbacks) {
  EXPECT_CALL(done_callback_, Run(_)).Times(0);
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  retry_timer.Start(base::TimeDelta::FromSeconds(10), CaptureCallback(),
                    done_callback_.Get());
  ASSERT_TRUE(captured_callback_);
  retry_timer.Cancel();
  std::move(captured_callback_).Run(OkClientStatus());  // Should do nothing
}

TEST_F(RetryTimerTest, GiveUpWhenLeavingScope) {
  EXPECT_CALL(done_callback_, Run(_)).Times(0);
  {
    RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
    retry_timer.Start(base::TimeDelta::FromSeconds(10), AlwaysFailsCallback(),
                      done_callback_.Get());
    EXPECT_EQ(1, try_count_);
  }
  FastForwardOneSecond();  // nothing should happen
}

TEST_F(RetryTimerTest, GiveUpWhenLeavingScopeWithPendingCallback) {
  EXPECT_CALL(done_callback_, Run(_)).Times(0);
  {
    RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
    retry_timer.Start(base::TimeDelta::FromSeconds(10), CaptureCallback(),
                      done_callback_.Get());
    ASSERT_TRUE(captured_callback_);
  }
  std::move(captured_callback_).Run(OkClientStatus());  // Should do nothing
}

TEST_F(RetryTimerTest, RestartOverridesFirstCall) {
  EXPECT_CALL(done_callback_, Run(_)).Times(0);

  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  retry_timer.Start(base::TimeDelta::FromSeconds(1), AlwaysFailsCallback(),
                    done_callback_.Get());
  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>>
      done_callback2;
  retry_timer.Start(base::TimeDelta::FromSeconds(1), AlwaysFailsCallback(),
                    done_callback2.Get());
  EXPECT_EQ(2, try_count_);
  EXPECT_CALL(done_callback2, Run(EqualsStatus(ClientStatus())));
  FastForwardOneSecond();
  EXPECT_EQ(3, try_count_);
}

TEST_F(RetryTimerTest, RestartOverridesFirstCallWithPendingTask) {
  EXPECT_CALL(done_callback_, Run(_)).Times(0);

  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  retry_timer.Start(base::TimeDelta::FromSeconds(1), CaptureCallback(),
                    done_callback_.Get());
  ASSERT_TRUE(captured_callback_);

  base::MockCallback<base::OnceCallback<void(const ClientStatus&)>>
      done_callback2;
  retry_timer.Start(base::TimeDelta::FromSeconds(1), AlwaysFailsCallback(),
                    done_callback2.Get());

  std::move(captured_callback_).Run(OkClientStatus());  // Should do nothing

  EXPECT_CALL(done_callback2, Run(EqualsStatus(ClientStatus())));
  FastForwardOneSecond();
  EXPECT_EQ(3, try_count_);
}

TEST_F(RetryTimerTest, Running) {
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(retry_timer.running());

  retry_timer.Start(base::TimeDelta::FromSeconds(10), SucceedsOnceCallback(1),
                    done_callback_.Get());
  EXPECT_TRUE(retry_timer.running());

  EXPECT_CALL(done_callback_, Run(EqualsStatus(OkClientStatus())));
  FastForwardOneSecond();
  EXPECT_FALSE(retry_timer.running());
}

TEST_F(RetryTimerTest, NotRunningAfterCancel) {
  RetryTimer retry_timer(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(retry_timer.running());

  retry_timer.Start(base::TimeDelta::FromSeconds(10), SucceedsOnceCallback(1),
                    done_callback_.Get());
  EXPECT_TRUE(retry_timer.running());
  retry_timer.Cancel();
  EXPECT_FALSE(retry_timer.running());
}

}  // namespace
}  // namespace autofill_assistant
