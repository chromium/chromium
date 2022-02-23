// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/wait_for_navigation_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::Property;
using ::testing::Return;

class WaitForNavigationActionTest : public testing::Test {
 protected:
  WaitForNavigationActionTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_wait_for_navigation() = proto_;
    action_ = std::make_unique<WaitForNavigationAction>(&mock_action_delegate_,
                                                        action_proto);
    action_->ProcessAction(callback_.Get());
  }

  base::test::TaskEnvironment task_environment_;

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  WaitForNavigationProto proto_;

 private:
  std::unique_ptr<WaitForNavigationAction> action_;
};

TEST_F(WaitForNavigationActionTest, FailsForUnexpectedNavigation) {
  EXPECT_CALL(mock_action_delegate_, WaitForNavigation).WillOnce(Return(false));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(WaitForNavigationActionTest, TimesOutWithoutNavigation) {
  EXPECT_CALL(mock_action_delegate_, WaitForNavigation).WillOnce(Return(true));
  EXPECT_CALL(callback_,
              Run(Pointee(Property(&ProcessedActionProto::status, TIMED_OUT))));

  proto_.set_timeout_ms(1000);
  Run();
  task_environment_.FastForwardBy(base::Seconds(2));
}

TEST_F(WaitForNavigationActionTest, ReturnsErrorStatusForNavigationError) {
  EXPECT_CALL(mock_action_delegate_, WaitForNavigation)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
        return true;
      });
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, NAVIGATION_ERROR))));

  proto_.set_timeout_ms(1000);
  Run();
}

TEST_F(WaitForNavigationActionTest, SucceedsForSuccessfulNavigation) {
  EXPECT_CALL(mock_action_delegate_, WaitForNavigation)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
        return true;
      });
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  proto_.set_timeout_ms(1000);
  Run();

  // Should not crash. The callback has already been called.
  task_environment_.FastForwardBy(base::Seconds(2));
}

}  // namespace
}  // namespace autofill_assistant
