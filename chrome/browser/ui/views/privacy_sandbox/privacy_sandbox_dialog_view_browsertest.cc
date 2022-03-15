// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

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

class PrivacySandboxDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    mock_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&BuildMockPrivacySandboxService)));
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
