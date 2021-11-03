// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/perform_on_single_element_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArgPointee;

class ExtendedMockActionDelegate : public MockActionDelegate {
 public:
  ExtendedMockActionDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  ElementStore* GetElementStore() const override {
    if (!element_store_) {
      element_store_ = std::make_unique<FakeElementStore>(web_contents_);
    }
    return element_store_.get();
  }

 private:
  content::WebContents* web_contents_;
};

const char kClientId[] = "1";
const char kUrl[] = "https://www.example.com";
const char kUsername[] = "username";

class PerformOnSingleElementActionTest : public testing::Test {
 public:
  PerformOnSingleElementActionTest() {}

  void SetUp() override {
    client_id_.set_identifier(kClientId);
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);

    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  base::MockCallback<PerformOnSingleElementAction::PerformAction> perform_;
  base::MockCallback<PerformOnSingleElementAction::PerformTimedAction>
      perform_timed_;
  UserData user_data_;
  ActionProto action_proto_;
  ClientIdProto client_id_;
};

TEST_F(PerformOnSingleElementActionTest, EmptyClientIdFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  EXPECT_CALL(perform_, Run).Times(0);

  ClientIdProto client_id;
  auto action = PerformOnSingleElementAction::WithClientId(
      &mock_action_delegate_, action_proto_, client_id, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest, OptionalEmptyClientIdDoesNotFail) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  EXPECT_CALL(perform_, Run).WillOnce(RunOnceCallback<1>(OkClientStatus()));

  ClientIdProto client_id;
  auto action = PerformOnSingleElementAction::WithOptionalClientId(
      &mock_action_delegate_, action_proto_, client_id, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest,
       OptionalEmptyClientIdDoesNotFailForTimed) {
  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));
  EXPECT_CALL(perform_timed_, Run)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(1)));

  ClientIdProto client_id;
  auto action = PerformOnSingleElementAction::WithOptionalClientIdTimed(
      &mock_action_delegate_, action_proto_, client_id, perform_timed_.Get());
  action->ProcessAction(callback_.Get());

  EXPECT_EQ(ACTION_APPLIED, capture.status());
  EXPECT_EQ(1000, capture.timing_stats().wait_time_ms());
}

TEST_F(PerformOnSingleElementActionTest, PasswordTextValueReturnLastTimeUsed) {
  ExtendedMockActionDelegate extended_mock_action_delegate(web_contents_.get());

  ON_CALL(extended_mock_action_delegate, GetWebsiteLoginManager)
      .WillByDefault(Return(&mock_website_login_manager_));

  EXPECT_CALL(extended_mock_action_delegate, GetUserData)
      .WillOnce(Return(&user_data_));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));

  EXPECT_CALL(perform_, Run).WillOnce(RunOnceCallback<1>(OkClientStatus()));

  ON_CALL(mock_website_login_manager_, GetGetLastTimePasswordUsed)
      .WillByDefault(RunOnceCallback<1>(base::Time::Now() - base::Days(32)));

  user_data_.selected_login_ =
      absl::make_optional<WebsiteLoginManager::Login>(GURL(kUrl), kUsername);

  ElementFinder::Result element;
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL(kUrl));
  element.dom_object.object_data.object_id = "id";
  element.container_frame_host = web_contents_->GetMainFrame();
  extended_mock_action_delegate.GetElementStore()->AddElement(
      kClientId, element.dom_object);

  action_proto_.mutable_send_keystroke_events()
      ->mutable_value()
      ->mutable_password_manager_value()
      ->set_credential_type(PasswordManagerValue::PASSWORD);

  auto action = PerformOnSingleElementAction::WithOptionalClientId(
      &extended_mock_action_delegate, action_proto_, client_id_,
      perform_.Get());
  action->ProcessAction(callback_.Get());

  EXPECT_EQ(1, capture.send_key_stroke_events_result()
                   .months_since_password_last_used());
}

TEST_F(PerformOnSingleElementActionTest,
       PasswordTextValueReturnLastTimeUsedFails) {
  ExtendedMockActionDelegate extended_mock_action_delegate(web_contents_.get());

  ON_CALL(extended_mock_action_delegate, GetWebsiteLoginManager)
      .WillByDefault(Return(&mock_website_login_manager_));

  EXPECT_CALL(extended_mock_action_delegate, GetUserData)
      .WillOnce(Return(&user_data_));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));

  EXPECT_CALL(perform_, Run).WillOnce(RunOnceCallback<1>(OkClientStatus()));
  ON_CALL(mock_website_login_manager_, GetGetLastTimePasswordUsed)
      .WillByDefault(RunOnceCallback<1>(absl::nullopt));

  user_data_.selected_login_ =
      absl::make_optional<WebsiteLoginManager::Login>(GURL(kUrl), kUsername);

  ElementFinder::Result element;
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL(kUrl));
  element.dom_object.object_data.object_id = "id";
  element.container_frame_host = web_contents_->GetMainFrame();
  extended_mock_action_delegate.GetElementStore()->AddElement(
      kClientId, element.dom_object);

  action_proto_.mutable_send_keystroke_events()
      ->mutable_value()
      ->mutable_password_manager_value()
      ->set_credential_type(PasswordManagerValue::PASSWORD);

  auto action = PerformOnSingleElementAction::WithOptionalClientId(
      &extended_mock_action_delegate, action_proto_, client_id_,
      perform_.Get());
  action->ProcessAction(callback_.Get());

  EXPECT_EQ(0, capture.send_key_stroke_events_result()
                   .months_since_password_last_used());
}

TEST_F(PerformOnSingleElementActionTest, FailsIfElementDoesNotExist) {
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));
  EXPECT_CALL(perform_, Run).Times(0);

  auto action = PerformOnSingleElementAction::WithClientId(
      &mock_action_delegate_, action_proto_, client_id_, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest, PerformsAndEnds) {
  ElementFinder::Result element;
  element.dom_object.object_data.object_id = "id";
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  EXPECT_CALL(perform_, Run(EqualsElement(element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));

  auto action = PerformOnSingleElementAction::WithClientId(
      &mock_action_delegate_, action_proto_, client_id_, perform_.Get());
  action->ProcessAction(callback_.Get());
}

TEST_F(PerformOnSingleElementActionTest, PerformsTimedAndEnds) {
  ElementFinder::Result element;
  element.dom_object.object_data.object_id = "id";
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object);

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));
  EXPECT_CALL(perform_timed_, Run(EqualsElement(element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), base::Seconds(1)));

  auto action = PerformOnSingleElementAction::WithClientIdTimed(
      &mock_action_delegate_, action_proto_, client_id_, perform_timed_.Get());
  action->ProcessAction(callback_.Get());

  EXPECT_EQ(ACTION_APPLIED, capture.status());
  EXPECT_EQ(1000, capture.timing_stats().wait_time_ms());
}

}  // namespace
}  // namespace autofill_assistant
