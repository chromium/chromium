// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kAverageBrowserWidth = 800;
constexpr int kAverageBrowserHeight = 700;

class MockPrivacySandboxService : public PrivacySandboxService {
 public:
  MOCK_METHOD(void,
              DialogActionOccurred,
              (PrivacySandboxService::DialogAction),
              (override));
};

std::unique_ptr<KeyedService> BuildMockPrivacySandboxService(
    content::BrowserContext* context) {
  return std::make_unique<::testing::NiceMock<MockPrivacySandboxService>>();
}

}  // namespace

class PrivacySandboxDialogViewBrowserTest : public DialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    mock_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&BuildMockPrivacySandboxService)));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    PrivacySandboxService::DialogType dialog_type =
        PrivacySandboxService::DialogType::kNone;
    if (name == "Consent") {
      dialog_type = PrivacySandboxService::DialogType::kConsent;
    }
    if (name == "Notice") {
      dialog_type = PrivacySandboxService::DialogType::kNotice;
    }
    ASSERT_NE(dialog_type, PrivacySandboxService::DialogType::kNone);

    // Resize the browser window to guarantee enough space for the dialog.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, kAverageBrowserWidth, kAverageBrowserHeight});

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        PrivacySandboxDialogView::kViewClassName);
    ShowPrivacySandboxDialog(browser(), dialog_type);
    waiter.WaitIfNeededAndGet();

    base::RunLoop().RunUntilIdle();
  }

  MockPrivacySandboxService* mock_service() { return mock_service_; }

 private:
  raw_ptr<MockPrivacySandboxService> mock_service_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest,
                       EscapeClosesNotice) {
  // Check that when the escape key is pressed, the notice is closed.
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(PrivacySandboxService::DialogAction::kNoticeShown));
  EXPECT_CALL(*mock_service(),
              DialogActionOccurred(
                  PrivacySandboxService::DialogAction::kNoticeDismiss));
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kNoticeClosedNoInteraction));
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);
  ShowPrivacySandboxDialog(browser(),
                           PrivacySandboxService::DialogType::kNotice);
  auto* dialog = waiter.WaitIfNeededAndGet();
  dialog->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  EXPECT_TRUE(dialog->IsClosed());

  // While IsClosed updated immediately, the widget will only actually close,
  // and thus inform the service asynchronously so must be waited for.
  base::RunLoop().RunUntilIdle();

  // Shutting down the browser test will naturally shut the dialog, verify
  // expectations before that happens.
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest,
                       EscapeDoesntCloseConsent) {
  // Check that when the escape key is pressed, the consent is not closed.
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(PrivacySandboxService::DialogAction::kConsentShown));
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kConsentClosedNoDecision))
      .Times(0);
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);
  ShowPrivacySandboxDialog(browser(),
                           PrivacySandboxService::DialogType::kConsent);
  auto* dialog = waiter.WaitIfNeededAndGet();
  dialog->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(dialog->IsClosed());

  // Shutting down the browser test will naturally shut the dialog, verify
  // expectations before that happens.
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_Consent DISABLED_InvokeUi_Consent
#else
#define MAYBE_InvokeUi_Consent InvokeUi_Consent
#endif
IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest,
                       MAYBE_InvokeUi_Consent) {
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(PrivacySandboxService::DialogAction::kConsentShown));
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kConsentClosedNoDecision));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest, InvokeUi_Notice) {
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(PrivacySandboxService::DialogAction::kNoticeShown));
  EXPECT_CALL(
      *mock_service(),
      DialogActionOccurred(
          PrivacySandboxService::DialogAction::kNoticeClosedNoInteraction));
  ShowAndVerifyUi();
}
