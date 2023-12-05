// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/ozone/buildflags.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

class PrivacySandboxDialogViewInteractiveUiTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    mock_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&BuildMockPrivacySandboxService)));
  }

  MockPrivacySandboxService* mock_service() { return mock_service_; }

 private:
  raw_ptr<MockPrivacySandboxService, DanglingUntriaged> mock_service_;
};

// The build flag OZONE_PLATFORM_WAYLAND is only available on
// Linux or ChromeOS, so this simplifies the next set of ifdefs.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(OZONE_PLATFORM_WAYLAND)
#define OZONE_PLATFORM_WAYLAND
#endif  // BUILDFLAG(OZONE_PLATFORM_WAYLAND)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/1315979): Flaky on most release builds.
// TODO(crbug.com/1430490): Broken on macOS
#if defined(NDEBUG) || BUILDFLAG(IS_CHROMEOS_ASH) || \
    defined(OZONE_PLATFORM_WAYLAND) || BUILDFLAG(IS_MAC)
#define MAYBE_EscapeClosesNotice DISABLED_EscapeClosesNotice
#else
#define MAYBE_EscapeClosesNotice EscapeClosesNotice
#endif
IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewInteractiveUiTest,
                       MAYBE_EscapeClosesNotice) {
  // Check that when the escape key is pressed, the notice is closed.
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown));
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kNoticeDismiss));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction))
      .Times(0);
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);
  ShowPrivacySandboxPrompt(browser(),
                           PrivacySandboxService::PromptType::kNotice);
  auto* dialog = waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(dialog);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  // The dialog can be in the deletion process at this stage, so don't check
  // if the dialog is closed. Instead, rely on the actions logged from the
  // service.
  base::RunLoop().RunUntilIdle();

  // Shutting down the browser test will naturally shut the dialog, verify
  // expectations before that happens.
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewInteractiveUiTest,
                       EscapeDoesntCloseConsent) {
  // Check that when the escape key is pressed, the consent is not closed.
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kConsentShown));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kConsentClosedNoDecision))
      .Times(0);
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);
  ShowPrivacySandboxDialog(browser(),
                           PrivacySandboxService::PromptType::kConsent);
  auto* dialog = waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(dialog->IsClosed());

  // Shutting down the browser test will naturally shut the dialog, verify
  // expectations before that happens.
  testing::Mock::VerifyAndClearExpectations(mock_service());
}

class PrivacySandboxDialogViewInteractiveUiTestM1
    : public InProcessBrowserTest {
 public:
  PrivacySandboxDialogViewInteractiveUiTestM1() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{privacy_sandbox::kDisablePrivacySandboxPrompts,
                               {}},
                              {privacy_sandbox::kPrivacySandboxSettings4,
                               {{"consent-required", "true"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewInteractiveUiTestM1,
                       MultipleDialogsClosed) {
  // Check that when the service receives indication that the flow has been
  // completed, that all open dialogs are closed.
  views::NamedWidgetShownWaiter waiter1(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);
  ShowPrivacySandboxDialog(browser(),
                           PrivacySandboxService::PromptType::kM1Consent);
  auto* dialog1 = waiter1.WaitIfNeededAndGet();

  views::NamedWidgetShownWaiter waiter2(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);
  auto* new_rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* new_browser = chrome::FindBrowserWithTab(
      content::WebContents::FromRenderFrameHost(new_rfh));
  ShowPrivacySandboxDialog(new_browser,
                           PrivacySandboxService::PromptType::kM1Consent);
  auto* dialog2 = waiter2.WaitIfNeededAndGet();

  // Check two distinct dialogs were opened.
  EXPECT_FALSE(dialog1->IsClosed());
  EXPECT_FALSE(dialog2->IsClosed());
  EXPECT_NE(dialog1, dialog2);

  // Completing a consent step shouldn't close the dialogs.
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());
  privacy_sandbox_service->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentShown);
  privacy_sandbox_service->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentAccepted);
  privacy_sandbox_service->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentDeclined);

  EXPECT_FALSE(dialog1->IsClosed());
  EXPECT_FALSE(dialog2->IsClosed());

  // While completing the notice step, should close all dialogs
  privacy_sandbox_service->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kNoticeAcknowledge);
  EXPECT_TRUE(dialog1->IsClosed());
  EXPECT_TRUE(dialog2->IsClosed());
}
