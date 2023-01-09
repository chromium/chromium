// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/callback_work_item.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A callback that always fails (returns false).
bool TestFailureCallback(const CallbackWorkItem& item) {
  return false;
}

}  // namespace

// Test that the work item returns false when a callback returns failure.
TEST(CallbackWorkItemTest, TestFailure) {
  CallbackWorkItem work_item(base::BindOnce(&TestFailureCallback),
                             base::DoNothing());

  EXPECT_FALSE(work_item.Do());
}

namespace {

enum TestCallbackState {
  TCS_UNDEFINED,
  TCS_CALLED_FORWARD,
  TCS_CALLED_ROLLBACK,
};

}  // namespace

// Test that the callback is invoked correctly during Do() and Rollback().
TEST(CallbackWorkItemTest, TestForwardBackward) {
  TestCallbackState state = TCS_UNDEFINED;

  CallbackWorkItem work_item(
      base::BindLambdaForTesting([&](const CallbackWorkItem& item) -> bool {
        state = TCS_CALLED_FORWARD;
        return true;
      }),
      base::BindLambdaForTesting(
          [&](const CallbackWorkItem& item) { state = TCS_CALLED_ROLLBACK; }));

  EXPECT_TRUE(work_item.Do());
  EXPECT_EQ(TCS_CALLED_FORWARD, state);

  work_item.Rollback();
  EXPECT_EQ(TCS_CALLED_ROLLBACK, state);
}
