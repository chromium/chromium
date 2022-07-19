// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/send_keystroke_events_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/public/password_change/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
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

const char kUrl[] = "https://www.example.com";
const char kUsername[] = "username";

class SendKeystrokeEventsActionTest : public testing::Test {
 public:
  SendKeystrokeEventsActionTest() {}

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);

    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_web_controller_, SendTextInput)
        .WillByDefault(RunOnceCallback<3>(OkClientStatus()));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_send_keystroke_events() = proto_;
    SendKeystrokeEventsAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  UserData user_data_;
  SendKeystrokeEventsProto proto_;
};

TEST_F(SendKeystrokeEventsActionTest, EmptyClientIdFails) {
  EXPECT_CALL(mock_web_controller_, SendTextInput).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(SendKeystrokeEventsActionTest, FailsIfElementDoesNotExist) {
  EXPECT_CALL(mock_web_controller_, SendTextInput).Times(0);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));

  proto_.mutable_client_id()->set_identifier("e");
  Run();
}

TEST_F(SendKeystrokeEventsActionTest, SendsText) {
  ElementFinderResult element;
  element.SetObjectId("id");
  mock_action_delegate_.GetElementStore()->AddElement("e",
                                                      element.dom_object());

  EXPECT_CALL(mock_web_controller_,
              SendTextInput(20, "Hello", EqualsElement(element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  proto_.mutable_client_id()->set_identifier("e");
  proto_.mutable_value()->set_text("Hello");
  proto_.set_delay_in_ms(20);
  Run();
}

TEST_F(SendKeystrokeEventsActionTest, PasswordTextValueReturnLastTimeUsed) {
  FakeElementStore fake_element_store(web_contents_.get());
  ON_CALL(mock_action_delegate_, GetElementStore)
      .WillByDefault(Return(&fake_element_store));

  ElementFinderResult element;
  // The password's origin is compared against the frame's. Navigate the frame
  // to set the matching origin.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL(kUrl));
  element.SetObjectId("id");
  element.SetRenderFrameHostForTest(web_contents_->GetPrimaryMainFrame());
  mock_action_delegate_.GetElementStore()->AddElement("e",
                                                      element.dom_object());

  EXPECT_CALL(mock_web_controller_,
              SendTextInput(0, "password", EqualsElement(element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));

  EXPECT_CALL(mock_website_login_manager_, GetGetLastTimePasswordUsed)
      .WillOnce(RunOnceCallback<1>(base::Time::Now() - base::Days(32)));
  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin)
      .WillOnce(RunOnceCallback<1>(true, "password"));

  user_data_.selected_login_ =
      absl::make_optional<WebsiteLoginManager::Login>(GURL(kUrl), kUsername);

  proto_.mutable_client_id()->set_identifier("e");
  proto_.mutable_value()->mutable_password_manager_value()->set_credential_type(
      PasswordManagerValue::PASSWORD);
  Run();

  EXPECT_EQ(ACTION_APPLIED, capture.status());
  EXPECT_EQ(1, capture.send_key_stroke_events_result()
                   .months_since_password_last_used());
}

TEST_F(SendKeystrokeEventsActionTest,
       PasswordTextValueSucceedsIfLastTimeUsedNotAvailable) {
  FakeElementStore fake_element_store(web_contents_.get());
  ON_CALL(mock_action_delegate_, GetElementStore)
      .WillByDefault(Return(&fake_element_store));

  ElementFinderResult element;
  // The password's origin is compared against the frame's. Navigate the frame
  // to set the matching origin.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL(kUrl));
  element.SetObjectId("id");
  element.SetRenderFrameHostForTest(web_contents_->GetPrimaryMainFrame());
  mock_action_delegate_.GetElementStore()->AddElement("e",
                                                      element.dom_object());

  EXPECT_CALL(mock_web_controller_,
              SendTextInput(0, "password", EqualsElement(element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run).WillOnce(testing::SaveArgPointee<0>(&capture));

  ON_CALL(mock_website_login_manager_, GetGetLastTimePasswordUsed)
      .WillByDefault(RunOnceCallback<1>(absl::nullopt));
  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin)
      .WillOnce(RunOnceCallback<1>(true, "password"));

  user_data_.selected_login_ =
      absl::make_optional<WebsiteLoginManager::Login>(GURL(kUrl), kUsername);

  proto_.mutable_client_id()->set_identifier("e");
  proto_.mutable_value()->mutable_password_manager_value()->set_credential_type(
      PasswordManagerValue::PASSWORD);
  Run();

  EXPECT_EQ(ACTION_APPLIED, capture.status());
  EXPECT_FALSE(capture.send_key_stroke_events_result()
                   .has_months_since_password_last_used());
}

}  // namespace
}  // namespace autofill_assistant
