// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockPrivacySandboxDialogView {
 public:
  MOCK_METHOD(void, Close, ());
  MOCK_METHOD(void, ResizeNativeView, (int));
  MOCK_METHOD(void, OpenPrivacySandboxSettings, ());
};

class MockPrivacySandboxService : public PrivacySandboxService {
 public:
  MOCK_METHOD(void,
              DialogActionOccurred,
              (PrivacySandboxService::DialogAction),
              (override));
};

std::unique_ptr<KeyedService> BuildMockPrivacySandboxService(
    content::BrowserContext* context) {
  return std::make_unique<::testing::StrictMock<MockPrivacySandboxService>>();
}

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

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  PrivacySandboxDialogHandler* handler() { return handler_.get(); }
  TestingProfile* profile() { return &profile_; }
  raw_ptr<MockPrivacySandboxDialogView> dialog_mock() {
    return dialog_mock_.get();
  }
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
    return std::make_unique<PrivacySandboxDialogHandler>(
        base::BindOnce(&MockPrivacySandboxDialogView::Close, dialog_mock()),
        base::BindOnce(&MockPrivacySandboxDialogView::ResizeNativeView,
                       dialog_mock()),
        base::BindOnce(
            &MockPrivacySandboxDialogView::OpenPrivacySandboxSettings,
            dialog_mock()),
        PrivacySandboxService::DialogType::kConsent);
  }
};

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleResizeDialog) {
  const int kDefaultDialogHeight = 350;
  EXPECT_CALL(*dialog_mock(), ResizeNativeView(kDefaultDialogHeight));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(PrivacySandboxService::DialogAction::kConsentShown));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kConsentClosedNoDecision));

  base::Value args(base::Value::Type::LIST);
  args.Append(kDefaultDialogHeight);
  handler()->HandleResizeDialog(args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleClickLearnMore) {
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogActionOccurred(
                  PrivacySandboxService::DialogAction::kConsentMoreInfoOpened));
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogActionOccurred(
                  PrivacySandboxService::DialogAction::kConsentMoreInfoClosed));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kConsentClosedNoDecision));

  base::Value more_info_opened_args(base::Value::Type::LIST);
  more_info_opened_args.Append(static_cast<int>(
      PrivacySandboxService::DialogAction::kConsentMoreInfoOpened));
  handler()->HandleDialogActionOccurred(
      more_info_opened_args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());

  base::Value more_info_closed_args(base::Value::Type::LIST);
  more_info_closed_args.Append(static_cast<int>(
      PrivacySandboxService::DialogAction::kConsentMoreInfoClosed));
  handler()->HandleDialogActionOccurred(
      more_info_closed_args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleConsentAccepted) {
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogActionOccurred(
                  PrivacySandboxService::DialogAction::kConsentAccepted));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kConsentClosedNoDecision))
      .Times(0);

  base::Value args(base::Value::Type::LIST);
  args.Append(
      static_cast<int>(PrivacySandboxService::DialogAction::kConsentAccepted));
  handler()->HandleDialogActionOccurred(args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxConsentDialogHandlerTest, HandleConsentDeclined) {
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogActionOccurred(
                  PrivacySandboxService::DialogAction::kConsentDeclined));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kConsentClosedNoDecision))
      .Times(0);

  base::Value args(base::Value::Type::LIST);
  args.Append(
      static_cast<int>(PrivacySandboxService::DialogAction::kConsentDeclined));
  handler()->HandleDialogActionOccurred(args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

class PrivacySandboxNoticeDialogHandlerTest
    : public PrivacySandboxDialogHandlerTest {
 protected:
  std::unique_ptr<PrivacySandboxDialogHandler> CreateHandler() override {
    return std::make_unique<PrivacySandboxDialogHandler>(
        base::BindOnce(&MockPrivacySandboxDialogView::Close, dialog_mock()),
        base::BindOnce(&MockPrivacySandboxDialogView::ResizeNativeView,
                       dialog_mock()),
        base::BindOnce(
            &MockPrivacySandboxDialogView::OpenPrivacySandboxSettings,
            dialog_mock()),
        PrivacySandboxService::DialogType::kNotice);
  }
};

TEST_F(PrivacySandboxNoticeDialogHandlerTest, HandleResizeDialog) {
  const int kDefaultDialogHeight = 350;
  EXPECT_CALL(*dialog_mock(), ResizeNativeView(kDefaultDialogHeight));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(PrivacySandboxService::DialogAction::kNoticeShown));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kNoticeClosedNoInteraction));

  base::Value args(base::Value::Type::LIST);
  args.Append(kDefaultDialogHeight);
  handler()->HandleResizeDialog(args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxNoticeDialogHandlerTest, HandleOpenSettings) {
  EXPECT_CALL(*dialog_mock(), OpenPrivacySandboxSettings());
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogActionOccurred(
                  PrivacySandboxService::DialogAction::kNoticeOpenSettings));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kNoticeClosedNoInteraction))
      .Times(0);

  base::Value args(base::Value::Type::LIST);
  args.Append(static_cast<int>(
      PrivacySandboxService::DialogAction::kNoticeOpenSettings));
  handler()->HandleDialogActionOccurred(args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(PrivacySandboxNoticeDialogHandlerTest, HandleNoticeAcknowledge) {
  EXPECT_CALL(*dialog_mock(), Close());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              DialogActionOccurred(
                  PrivacySandboxService::DialogAction::kNoticeAcknowledge));
  EXPECT_CALL(
      *mock_privacy_sandbox_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kNoticeClosedNoInteraction))
      .Times(0);

  base::Value args(base::Value::Type::LIST);
  args.Append(static_cast<int>(
      PrivacySandboxService::DialogAction::kNoticeAcknowledge));
  handler()->HandleDialogActionOccurred(args.GetListDeprecated());

  ASSERT_EQ(0U, web_ui()->call_data().size());
}
