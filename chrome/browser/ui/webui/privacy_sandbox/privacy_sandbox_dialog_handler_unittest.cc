// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"

#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kCallbackId[] = "test-callback-id";

class MockPrivacySandboxDialogView {
 public:
  MOCK_METHOD(void, Close, ());
  MOCK_METHOD(void, ResizeNativeView, (int));
  MOCK_METHOD(void, ShowNativeView, ());
  MOCK_METHOD(void, OpenPrivacySandboxSettings, ());
  MOCK_METHOD(void, OpenPrivacySandboxAdMeasurementSettings, ());
};

}  // namespace

class PrivacySandboxDialogHandlerTest : public testing::Test {
 public:
  PrivacySandboxDialogHandlerTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    dialog_mock_ =
        std::make_unique<testing::StrictMock<MockPrivacySandboxDialogView>>();
    mock_privacy_sandbox_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockPrivacySandboxService)));

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
    handler_ = CreateHandler();
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui_->ClearTrackedCalls();
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();
  }

  // Used when validating those prompt types that only notify of a prompt action
  // on the WebUI side.
  void ShowDialog() {
    EXPECT_CALL(*dialog_mock(), ShowNativeView());
    base::Value::List args;
    handler()->HandleShowDialog(args);
  }

  // Same as the no-arg variant, but also validates that the mock service
  // received a prompt action (for those prompts that publish the event in the
  // handler rather than WebUI).
  void ShowDialog(PrivacySandboxService::PromptAction expected_action) {
    EXPECT_CALL(*mock_privacy_sandbox_service(),
                PromptActionOccurred(expected_action));
    ShowDialog();
  }

  void IdempotentPromptActionOccurred(const base::Value::List& args) {
    // Inform the handler multiple times that a prompt action occurred. The test
    // using this function expects the call to be idempotent.
    handler()->HandlePromptActionOccurred(args);
    handler()->HandlePromptActionOccurred(args);
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  PrivacySandboxDialogHandler* handler() { return handler_.get(); }
  TestingProfile* profile() { return &profile_; }
  MockPrivacySandboxDialogView* dialog_mock() { return dialog_mock_.get(); }
  MockPrivacySandboxService* mock_privacy_sandbox_service() {
    return mock_privacy_sandbox_service_;
  }

 protected:
  virtual std::unique_ptr<PrivacySandboxDialogHandler> CreateHandler() = 0;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_ =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<PrivacySandboxDialogHandler> handler_;
  std::unique_ptr<MockPrivacySandboxDialogView> dialog_mock_;
  raw_ptr<MockPrivacySandboxService> mock_privacy_sandbox_service_;
};

class PrivacySandboxConsentDialogHandlerTest
    : public PrivacySandboxDialogHandlerTest {
 protected:
  std::unique_ptr<PrivacySandboxDialogHandler> CreateHandler() override {
    // base::Unretained is safe because the created handler does not outlive the
    // mock.
    return std::make_unique<PrivacySandboxDialogHandler>(
        base::BindOnce(&MockPrivacySandboxDialogView::Close,
                       base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::ResizeNativeView,
                       base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::ShowNativeView,
                       base::Unretained(dialog_mock())),
        base::BindOnce(
            &MockPrivacySandboxDialogView::OpenPrivacySandboxSettings,
            base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::
                           OpenPrivacySandboxAdMeasurementSettings,
                       base::Unretained(dialog_mock())),
        PrivacySandboxService::PromptType::kConsent);
  }
};

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleResizeDialog) {
  const int kDefaultDialogHeight = 350;
  EXPECT_CALL(*dialog_mock(), ResizeNativeView(kDefaultDialogHeight));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kConsentClosedNoDecision));

  base::Value::List args;
  args.Append(kCallbackId);
  args.Append(kDefaultDialogHeight);
  handler()->HandleResizeDialog(args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleShowDialog) {
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kConsentClosedNoDecision));

  ShowDialog(PrivacySandboxService::PromptAction::kConsentShown);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleClickLearnMore) {
  ShowDialog(PrivacySandboxService::PromptAction::kConsentShown);
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentMoreInfoOpened));
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentMoreInfoClosed));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kConsentClosedNoDecision));

  base::Value::List more_info_opened_args;
  more_info_opened_args.Append(static_cast<int>(
      PrivacySandboxService::PromptAction::kConsentMoreInfoOpened));
  handler()->HandlePromptActionOccurred(more_info_opened_args);

  ASSERT_EQ(0U, web_ui()->call_data().size());

  base::Value::List more_info_closed_args;
  more_info_closed_args.Append(static_cast<int>(
      PrivacySandboxService::PromptAction::kConsentMoreInfoClosed));
  handler()->HandlePromptActionOccurred(more_info_closed_args);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleConsentAccepted) {
  ShowDialog(PrivacySandboxService::PromptAction::kConsentShown);
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentAccepted));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kConsentClosedNoDecision))
      .Times(0);

  base::Value::List args;
  args.Append(
      static_cast<int>(PrivacySandboxService::PromptAction::kConsentAccepted));
  IdempotentPromptActionOccurred(args);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleConsentDeclined) {
  ShowDialog(PrivacySandboxService::PromptAction::kConsentShown);
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentDeclined));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kConsentClosedNoDecision))
      .Times(0);

  base::Value::List args;
  args.Append(
      static_cast<int>(PrivacySandboxService::PromptAction::kConsentDeclined));
  IdempotentPromptActionOccurred(args);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest,
       NotifyServiceAboutPromptAction_Invokes_PromptActionOccured) {
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kConsentShown));
  handler()->NotifyServiceAboutPromptAction(
      PrivacySandboxService::PromptAction::kConsentShown);

  // This is needed because PromptActionOccurred is called again when
  // PrivacySandboxDialogHandler is destroyed in tearDown.
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kConsentClosedNoDecision));
}

class PrivacySandboxNoticeDialogHandlerTest
    : public PrivacySandboxDialogHandlerTest {
 protected:
  std::unique_ptr<PrivacySandboxDialogHandler> CreateHandler() override {
    // base::Unretained is safe because the created handler does not outlive the
    // mock.
    return std::make_unique<PrivacySandboxDialogHandler>(
        base::BindOnce(&MockPrivacySandboxDialogView::Close,
                       base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::ResizeNativeView,
                       base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::ShowNativeView,
                       base::Unretained(dialog_mock())),
        base::BindOnce(
            &MockPrivacySandboxDialogView::OpenPrivacySandboxSettings,
            base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::
                           OpenPrivacySandboxAdMeasurementSettings,
                       base::Unretained(dialog_mock())),
        PrivacySandboxService::PromptType::kNotice);
  }
};

TEST_F(PrivacySandboxNoticeDialogHandlerTest, HandleResizeDialog) {
  const int kDefaultDialogHeight = 350;
  EXPECT_CALL(*dialog_mock(), ResizeNativeView(kDefaultDialogHeight));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction));

  base::Value::List args;
  args.Append(kCallbackId);
  args.Append(kDefaultDialogHeight);
  handler()->HandleResizeDialog(args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
}

TEST_F(PrivacySandboxNoticeDialogHandlerTest, HandleShowDialog) {
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction));
  ShowDialog(PrivacySandboxService::PromptAction::kNoticeShown);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxNoticeDialogHandlerTest, HandleOpenSettings) {
  ShowDialog(PrivacySandboxService::PromptAction::kNoticeShown);
  EXPECT_CALL(*dialog_mock(), OpenPrivacySandboxSettings());
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeOpenSettings));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);

  base::Value::List args;
  args.Append(static_cast<int>(
      PrivacySandboxService::PromptAction::kNoticeOpenSettings));
  IdempotentPromptActionOccurred(args);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxNoticeDialogHandlerTest, HandleNoticeAcknowledge) {
  ShowDialog(PrivacySandboxService::PromptAction::kNoticeShown);
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeAcknowledge));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);

  base::Value::List args;
  args.Append(static_cast<int>(
      PrivacySandboxService::PromptAction::kNoticeAcknowledge));
  IdempotentPromptActionOccurred(args);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxNoticeDialogHandlerTest,
       NotifyServiceAboutPromptAction_Invokes_PromptActionOccured) {
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  handler()->NotifyServiceAboutPromptAction(
      PrivacySandboxService::PromptAction::kNoticeShown);

  // This is needed because PromptActionOccurred is called again when
  // PrivacySandboxDialogHandler is destroyed in tearDown.
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction));
}

class PrivacySandboxNoticeRestrictedDialogHandlerTest
    : public PrivacySandboxDialogHandlerTest {
 protected:
  std::unique_ptr<PrivacySandboxDialogHandler> CreateHandler() override {
    // base::Unretained is safe because the created handler does not outlive the
    // mock.
    return std::make_unique<PrivacySandboxDialogHandler>(
        base::BindOnce(&MockPrivacySandboxDialogView::Close,
                       base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::ResizeNativeView,
                       base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::ShowNativeView,
                       base::Unretained(dialog_mock())),
        base::BindOnce(
            &MockPrivacySandboxDialogView::OpenPrivacySandboxSettings,
            base::Unretained(dialog_mock())),
        base::BindOnce(&MockPrivacySandboxDialogView::
                           OpenPrivacySandboxAdMeasurementSettings,
                       base::Unretained(dialog_mock())),
        PrivacySandboxService::PromptType::kM1NoticeRestricted);
  }
};

TEST_F(PrivacySandboxNoticeRestrictedDialogHandlerTest, HandleOpenSettings) {
  ShowDialog();
  EXPECT_CALL(*dialog_mock(), OpenPrivacySandboxAdMeasurementSettings());
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kRestrictedNoticeOpenSettings));
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(PrivacySandboxService::PromptAction::
                                       kRestrictedNoticeClosedNoInteraction))
      .Times(0);

  base::Value::List args;
  args.Append(static_cast<int>(
      PrivacySandboxService::PromptAction::kRestrictedNoticeOpenSettings));
  IdempotentPromptActionOccurred(args);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxNoticeRestrictedDialogHandlerTest, HandleAcknowledge) {
  ShowDialog();
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kRestrictedNoticeAcknowledge));
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(PrivacySandboxService::PromptAction::
                                       kRestrictedNoticeClosedNoInteraction))
      .Times(0);

  base::Value::List args;
  args.Append(static_cast<int>(
      PrivacySandboxService::PromptAction::kRestrictedNoticeAcknowledge));
  IdempotentPromptActionOccurred(args);

  ASSERT_EQ(0U, web_ui()->call_data().size());
}
