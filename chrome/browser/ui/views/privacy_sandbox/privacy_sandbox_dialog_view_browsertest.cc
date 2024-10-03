// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kAverageBrowserWidth = 800;
constexpr int kAverageBrowserHeight = 700;

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
    PrivacySandboxService::PromptType prompt_type =
        PrivacySandboxService::PromptType::kNone;
    if (name == "Consent") {
      prompt_type = PrivacySandboxService::PromptType::kM1Consent;
    }
    if (name == "Notice") {
      prompt_type = PrivacySandboxService::PromptType::kM1NoticeROW;
    }
    if (name == "RestrictedNotice") {
      prompt_type = PrivacySandboxService::PromptType::kM1NoticeRestricted;
    }
    ASSERT_NE(prompt_type, PrivacySandboxService::PromptType::kNone);

    // Resize the browser window to guarantee enough space for the dialog.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, kAverageBrowserWidth, kAverageBrowserHeight});

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        PrivacySandboxDialogView::kViewClassName);
    ShowPrivacySandboxDialog(browser(), prompt_type);

    auto* dialog_widget = static_cast<PrivacySandboxDialogView*>(
        waiter.WaitIfNeededAndGet()->widget_delegate()->GetContentsView());

    // TODO(crbug.com/41483512): Waiting for the document to exist before
    // performing the scroll action fixes the flakiness but we should try find a
    // better approach.
    ASSERT_TRUE(base::test::RunUntil([&] {
      return content::EvalJs(dialog_widget->GetWebContentsForTesting(),
                             "!!document")
          .ExtractBool();
    }));

    // Ensure dialog is fully scrolled, this is needed in order for the "*Shown"
    // action to be fired.
    auto scroll = content::EvalJs(dialog_widget->GetWebContentsForTesting(),
                                  "scrollTo(0, 1500)");

    base::RunLoop().RunUntilIdle();
  }

  MockPrivacySandboxService* mock_service() { return mock_service_; }

 private:
  raw_ptr<MockPrivacySandboxService, DanglingUntriaged> mock_service_;
};

// TODO(crbug.com/41484188): Re-enable the test.
// TODO(chrstne): temporarily enabling the test to debug cause of flakiness.
IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest, InvokeUi_Consent) {
  // TODO(chrstne): uncomment out once the source of the flakiness is resolved.
  /*EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kConsentShown,
                           PrivacySandboxService::SurfaceType::kDesktop));
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentClosedNoDecision,
                  PrivacySandboxService::SurfaceType::kDesktop));*/
  ShowAndVerifyUi();
}

// TODO(crbug.com/325436918): Re-enable the test.
IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest,
                       DISABLED_InvokeUi_Notice) {
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown,
                           PrivacySandboxService::SurfaceType::kDesktop));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction,
          PrivacySandboxService::SurfaceType::kDesktop));
  ShowAndVerifyUi();
}

// TODO(crbug.com/333163287): Re-enable the test.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_InvokeUi_RestrictedNotice DISABLED_InvokeUi_RestrictedNotice
#else
#define MAYBE_InvokeUi_RestrictedNotice InvokeUi_RestrictedNotice
#endif
IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest,
                       MAYBE_InvokeUi_RestrictedNotice) {
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kRestrictedNoticeShown,
                  PrivacySandboxService::SurfaceType::kDesktop));
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::
                               kRestrictedNoticeClosedNoInteraction,
                           PrivacySandboxService::SurfaceType::kDesktop));
  ShowAndVerifyUi();
}
