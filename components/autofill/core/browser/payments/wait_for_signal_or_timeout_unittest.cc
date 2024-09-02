// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wait_for_signal_or_timeout.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

class WaitForSignalOrTimeoutTest : public testing::Test {
 public:
  WaitForSignalOrTimeoutTest() = default;
  ~WaitForSignalOrTimeoutTest() override = default;

  WaitForSignalOrTimeout::Callback GetCallback() {
    return base::BindOnce(&WaitForSignalOrTimeoutTest::Callback,
                          base::Unretained(this));
  }

 protected:
  void Callback(bool triggered_by_signal) {
    callbacks_++;
    last_callback_triggered_by_signal_ = triggered_by_signal;
  }

  // Number of observed callbacks.
  int callbacks_ = 0;

  bool last_callback_triggered_by_signal_ = false;

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// WaitForSignalOrTimeout is initialized with a callback and then the Signal()
// happens.
TEST_F(WaitForSignalOrTimeoutTest, InitThenSignal) {
  WaitForSignalOrTimeout wait;
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  EXPECT_EQ(0, callbacks_);
  EXPECT_FALSE(wait.IsSignaled());
  wait.Signal();
  EXPECT_EQ(1, callbacks_);
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_TRUE(last_callback_triggered_by_signal_);

  // Another signal call should be ignored.
  wait.Signal();
  EXPECT_EQ(1, callbacks_);
  EXPECT_TRUE(wait.IsSignaled());

  // Also the pending timeout should not trigger further callbacks.
  task_env_.FastForwardBy(base::Seconds(35));
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_EQ(1, callbacks_);
}

// A Signal() is registered before the callback.
TEST_F(WaitForSignalOrTimeoutTest, SignalThenInit) {
  WaitForSignalOrTimeout wait;
  EXPECT_FALSE(wait.IsSignaled());

  // Trigger the signal before a callback handler is registered.
  wait.Signal();
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_EQ(0, callbacks_);

  // Once the callback handler is registered, it should be called immediately.
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_EQ(1, callbacks_);
  EXPECT_TRUE(last_callback_triggered_by_signal_);

  // Another signal call should be ignored.
  wait.Signal();
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_EQ(1, callbacks_);

  // Also the pending timeout should not trigger further callbacks.
  task_env_.FastForwardBy(base::Seconds(35));
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_EQ(1, callbacks_);
}

// A timeout occurs before Signal() is called.
TEST_F(WaitForSignalOrTimeoutTest, InitThenTimeout) {
  WaitForSignalOrTimeout wait;
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  EXPECT_FALSE(wait.IsSignaled());
  EXPECT_EQ(0, callbacks_);

  task_env_.FastForwardBy(base::Seconds(35));
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_EQ(1, callbacks_);
  EXPECT_FALSE(last_callback_triggered_by_signal_);

  // A late signal will be ignored.
  wait.Signal();
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_EQ(1, callbacks_);
}

// The WaitForSignalOrTimeout gets destroyed before a Signal() or timeout
// happens.
TEST_F(WaitForSignalOrTimeoutTest, DestroyedBeforeSignal) {
  {
    WaitForSignalOrTimeout wait;
    wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  }
  EXPECT_EQ(0, callbacks_);
  task_env_.FastForwardBy(base::Seconds(35));
  EXPECT_EQ(0, callbacks_);
}

// The WaitForSignalOrTimeout gets signaled, reset, and signaled again.
TEST_F(WaitForSignalOrTimeoutTest, Reset) {
  WaitForSignalOrTimeout wait;
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  EXPECT_EQ(0, callbacks_);
  wait.Signal();
  EXPECT_EQ(1, callbacks_);
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_TRUE(last_callback_triggered_by_signal_);

  wait.Reset();

  EXPECT_FALSE(wait.IsSignaled());

  // This signal does not trigger a callback because none is registered.
  wait.Signal();
  EXPECT_EQ(1, callbacks_);
  // Now the callback happens immediately.
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  EXPECT_EQ(2, callbacks_);
  EXPECT_TRUE(last_callback_triggered_by_signal_);

  wait.Reset();

  // Finally, we simulate a timeout after the reset.
  EXPECT_FALSE(wait.IsSignaled());
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  task_env_.FastForwardBy(base::Seconds(35));
  EXPECT_EQ(3, callbacks_);
  EXPECT_FALSE(last_callback_triggered_by_signal_);
}

TEST_F(WaitForSignalOrTimeoutTest, OnEventOrTimeOutCalledTwice) {
  WaitForSignalOrTimeout wait;
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));
  EXPECT_EQ(0, callbacks_);

  // Wait some time but not long enough for the timeout to trigger.
  task_env_.FastForwardBy(base::Seconds(25));
  EXPECT_EQ(0, callbacks_);
  EXPECT_FALSE(wait.IsSignaled());

  // This resets the state machine (currently waiting for a signal or timeout)
  // and starts a new wait.
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));

  // Wait some time but not long enough for the timeout to trigger.
  task_env_.FastForwardBy(base::Seconds(25));
  // The first timeout should not have triggered anything.
  EXPECT_EQ(0, callbacks_);
  EXPECT_FALSE(wait.IsSignaled());

  // Wait some more time for the second timeout to kick in.
  task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(1, callbacks_);
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_FALSE(last_callback_triggered_by_signal_);

  // This resets the state machine (currently in done state) once more and
  // starts a new wait.
  wait.OnEventOrTimeOut(GetCallback(), base::Seconds(30));

  // Wait some time but not long enough for the timeout to trigger.
  task_env_.FastForwardBy(base::Seconds(25));
  // The first timeout should not have triggered anything.
  EXPECT_EQ(1, callbacks_);
  EXPECT_FALSE(wait.IsSignaled());

  // Wait some more time for the second timeout to kick in.
  task_env_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(2, callbacks_);
  EXPECT_TRUE(wait.IsSignaled());
  EXPECT_FALSE(last_callback_triggered_by_signal_);
}

}  // namespace
}  // namespace autofill
