// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_ui_action_delegate.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/public/assistant_ui/proto/assistant_ui_action.pb.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

using ::testing::ElementsAre;

class AssistantUiActionDelegateTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::MockCallback<
      base::OnceCallback<void(ExternalActionDelegate::DomUpdateCallback)>>
      start_dom_checks_callback_;
  base::MockCallback<base::OnceCallback<void(const external::Result& result)>>
      end_action_callback_;
  AssistantUiActionDelegate delegate_;

  void RunOnActionRequested(const assistant_ui::AssistantUiAction& action) {
    external::Action external_action;
    *external_action.mutable_info()->mutable_assistant_ui_action() = action;
    delegate_.OnActionRequested(external_action,
                                /* is_interrupt= */ false,
                                start_dom_checks_callback_.Get(),
                                end_action_callback_.Get());
  }
};

TEST_F(AssistantUiActionDelegateTest, ContinueImmediately) {
  EXPECT_CALL(start_dom_checks_callback_, Run).Times(0);
  external::Result result;
  EXPECT_CALL(end_action_callback_, Run).WillOnce(testing::SaveArg<0>(&result));

  assistant_ui::AssistantUiAction action;
  action.mutable_continue_immediately();
  RunOnActionRequested(action);
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.result_info().assistant_ui_action_result().immediate());
}

TEST_F(AssistantUiActionDelegateTest, BlockUntilUserActionWithTimeout) {
  EXPECT_CALL(start_dom_checks_callback_, Run).Times(0);

  assistant_ui::AssistantUiAction action;
  action.mutable_block_until_user_action()->set_timeout_ms(100);
  RunOnActionRequested(action);

  external::Result result;
  EXPECT_CALL(end_action_callback_, Run).WillOnce(testing::SaveArg<0>(&result));
  task_environment_.FastForwardBy(base::Milliseconds(100));

  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.result_info().assistant_ui_action_result().timeout());
}

TEST_F(AssistantUiActionDelegateTest, BlockUntilUserActionWithDomChecks) {
  assistant_ui::AssistantUiAction action;
  action.mutable_block_until_user_action()->set_timeout_ms(100);
  action.mutable_block_until_user_action()->set_check_dom_conditions(true);

  ExternalActionDelegate::DomUpdateCallback dom_update_callback;
  EXPECT_CALL(start_dom_checks_callback_, Run)
      .WillOnce([&dom_update_callback](
                    ExternalActionDelegate::DomUpdateCallback callback) {
        dom_update_callback = std::move(callback);
      });

  RunOnActionRequested(action);

  external::ElementConditionsUpdate update;
  auto* result1 = update.add_results();
  result1->set_satisfied(false);
  result1->set_id(3);
  auto* result2 = update.add_results();
  result2->set_satisfied(false);
  result2->set_id(5);

  // Should not finish because none of the conditions are satisfied
  EXPECT_CALL(end_action_callback_, Run).Times(0);
  dom_update_callback.Run(update);

  result1->set_satisfied(true);
  external::Result result;
  EXPECT_CALL(end_action_callback_, Run).WillOnce(testing::SaveArg<0>(&result));
  dom_update_callback.Run(update);
  EXPECT_TRUE(result.success());
  const auto& action_result = result.result_info().assistant_ui_action_result();
  EXPECT_EQ(action_result.response_type_case(),
            assistant_ui::AssistantUiActionResult::kDomConditions);
  EXPECT_THAT(action_result.dom_conditions().condition_id(), ElementsAre(3));
}

}  // namespace
}  // namespace autofill_assistant
