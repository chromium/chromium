// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "base/synchronization/waitable_event.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
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
#if !BUILDFLAG(IS_LINUX)
constexpr base::TimeDelta kMaxWaitTime = base::Seconds(30);
#endif
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
    waiter.WaitIfNeededAndGet();
  }

  MockPrivacySandboxService* mock_service() { return mock_service_; }

 private:
  raw_ptr<MockPrivacySandboxService, DanglingUntriaged> mock_service_;
};

#if !BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest, InvokeUi_Consent) {
  base::WaitableEvent shown_waiter;
  base::WaitableEvent closed_waiter;

  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kConsentShown,
                           PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&shown_waiter] { shown_waiter.Signal(); });
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentClosedNoDecision,
                  PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&closed_waiter] { closed_waiter.Signal(); });
  ShowAndVerifyUi();

  LOG(INFO) << "Waiting for callback";
  shown_waiter.TimedWait(kMaxWaitTime);
  closed_waiter.TimedWait(kMaxWaitTime);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest, InvokeUi_Notice) {
  base::WaitableEvent shown_waiter;
  base::WaitableEvent closed_waiter;

  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown,
                           PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&shown_waiter] { shown_waiter.Signal(); });
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction,
          PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&closed_waiter] { closed_waiter.Signal(); });
  ShowAndVerifyUi();

  LOG(INFO) << "Waiting for callback";
  shown_waiter.TimedWait(kMaxWaitTime);
  closed_waiter.TimedWait(kMaxWaitTime);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest,
                       InvokeUi_RestrictedNotice) {
  base::WaitableEvent shown_waiter;
  base::WaitableEvent closed_waiter;

  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kRestrictedNoticeShown,
                  PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&shown_waiter] { shown_waiter.Signal(); });
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::
                               kRestrictedNoticeClosedNoInteraction,
                           PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&closed_waiter] { closed_waiter.Signal(); });
  ShowAndVerifyUi();

  LOG(INFO) << "Waiting for callback";
  shown_waiter.TimedWait(kMaxWaitTime);
  closed_waiter.TimedWait(kMaxWaitTime);
}
#endif
