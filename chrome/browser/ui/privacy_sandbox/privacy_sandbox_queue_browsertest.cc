// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt_helper.h"
#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

// This file is meant to test the general code path that causes notices to show.
// Specifically triggering of the notice queue.
class PrivacySandboxQueueTestHelper : public InProcessBrowserTest {
 public:
  content::WebContents* web_contents(PrivacySandboxDialogView* dialog_widget) {
    return dialog_widget->GetWebContentsForTesting();
  }

  void SetUpPrivacySandboxService() {
    auto* privacy_sandbox_service =
        PrivacySandboxServiceFactory::GetForProfile(browser()->profile());
    privacy_sandbox_service->SetPromptDisabledForTests(false);
    privacy_sandbox_service->ForceChromeBuildForTests(true);
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "be");
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  }

  void SetUpPrivacySandboxServiceAndDMA() {
    auto* privacy_sandbox_service =
        PrivacySandboxServiceFactory::GetForProfile(browser()->profile());
    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);
    privacy_sandbox_service->SetPromptDisabledForTests(false);
    privacy_sandbox_service->ForceChromeBuildForTests(true);
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "be");
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  }
};

class PrivacySandboxQueueTestNotice : public PrivacySandboxQueueTestHelper {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kPrivacySandboxNoticeQueue}, {});
  }

  std::string element_path_ =
      "document.querySelector('privacy-sandbox-combined-dialog-app')."
      "shadowRoot.querySelector('#notice').shadowRoot.querySelector('#"
      "ackButton')";
  std::string wait_for_button_ =
      "const element = " + element_path_ +
      ";"
      "const style = window.getComputedStyle(element);"
      "style.getPropertyValue('visibility');";

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Navigate to a invalid then valid webpage. Ensure handle is held throughout.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice, NoPrompt) {
  // Set flags correctly
  base::UserActionTester user_action_tester;
  SetUpPrivacySandboxService();
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

  // Navigate to invalid page.
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // When we navigate to a page that doesn't require a prompt, we should still
  // hog handle.
  ASSERT_TRUE(privacy_sandbox_service->IsHoldingHandle());

  // Navigate to valid page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_TRUE(privacy_sandbox_service->IsHoldingHandle());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.QueueOnStartup"),
            1);
}

// Navigate to a valid webpage (settings page) and click a notice. One window.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice, PromptShows) {
  // Set flags correctly.
  base::UserActionTester user_action_tester;
  SetUpPrivacySandboxService();
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

  // Navigate.
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  auto* dialog_widget = static_cast<PrivacySandboxDialogView*>(
      waiter.WaitIfNeededAndGet()->widget_delegate()->GetContentsView());

  // Before we click, should still be holding handle.
  ASSERT_TRUE(privacy_sandbox_service->IsHoldingHandle());

  // Click ack button.
  const std::string& code = element_path_ + ".click();";
  EXPECT_TRUE(content::ExecJs(
      PrivacySandboxQueueTestHelper::web_contents(dialog_widget), code,
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));

  // After click, should release handle.
  ASSERT_FALSE(privacy_sandbox_service->IsHoldingHandle());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.QueueOnStartup"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.ReleaseOnShown"),
            1);
}

// Navigate to a valid webpage (settings page) and click a notice. Two windows.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice,
                       PromptShowsMultipleWindows) {
  base::UserActionTester user_action_tester;
  // Set flags correctly.
  SetUpPrivacySandboxService();
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

  // Navigate.
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  auto* dialog_widget = static_cast<PrivacySandboxDialogView*>(
      waiter.WaitIfNeededAndGet()->widget_delegate()->GetContentsView());

  // After first nav, we should be holding handle.
  ASSERT_TRUE(privacy_sandbox_service->IsHoldingHandle());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav, should still be holding handle.
  ASSERT_TRUE(privacy_sandbox_service->IsHoldingHandle());

  // Click ack button on one window.
  const std::string& code = element_path_ + ".click();";
  EXPECT_TRUE(content::ExecJs(
      PrivacySandboxQueueTestHelper::web_contents(dialog_widget), code,
      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));

  // After click, should release handle.
  ASSERT_FALSE(privacy_sandbox_service->IsHoldingHandle());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.QueueOnStartup"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.ReleaseOnShown"),
            1);
}

// Browser startup assumes we don't need a notice. Then we need a notice.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice, DontNeedThenNeed) {
  base::UserActionTester user_action_tester;
  // Set flags incorrectly so we don't need a prompt.
  SetUpPrivacySandboxService();
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());
  privacy_sandbox_service->SetPromptDisabledForTests(true);

  // Navigate.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After first nav, we should not be holding handle.
  ASSERT_FALSE(privacy_sandbox_service->IsHoldingHandle());
  ASSERT_FALSE(privacy_sandbox_service->IsNoticeQueued());

  // Set the correct flag.
  privacy_sandbox_service->SetPromptDisabledForTests(false);

  // Navigate again and now we want to queue and hold the handle.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav, should have been queued and holding handle.
  ASSERT_TRUE(privacy_sandbox_service->IsHoldingHandle());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.QueueOnThOrNav"),
            1);
}

// Browser startup assumes we need a notice. Then we realize we don't need it.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice, NeedThenDontNeed) {
  base::UserActionTester user_action_tester;
  // Set flags correctly.
  SetUpPrivacySandboxService();
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

  // Navigate.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After first trigger, should have been queued and holding handle.
  ASSERT_TRUE(privacy_sandbox_service->IsHoldingHandle());

  // Change our mind about wanting a prompt.
  privacy_sandbox_service->SetPromptDisabledForTests(true);

  // Navigate.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav do not hold handle.
  ASSERT_FALSE(privacy_sandbox_service->IsNoticeQueued());
  ASSERT_FALSE(privacy_sandbox_service->IsHoldingHandle());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.QueueOnStartup"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.ReleaseOnThOrNav"),
            1);
}

class PrivacySandboxQueueTestNoticeWithSearchEngine
    : public PrivacySandboxQueueTestNotice {
  // Override the country to simulate showing the search engine choice dialog.
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");
    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
  }

 private:
  base::AutoReset<bool> scoped_chrome_build_override_ =
      SearchEngineChoiceDialogServiceFactory::
          ScopedChromeBuildOverrideForTesting(
              /*force_chrome_build=*/true);
};

// Navigate to a page where the DMA notice should show and ensure suppression.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNoticeWithSearchEngine,
                       PromptSuppressed) {
  base::UserActionTester user_action_tester;
  // Set flags correctly.
  SetUpPrivacySandboxServiceAndDMA();
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

  // When we navigate to valid page for SE dialog, we should unqueue and set the
  // suppress flag.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_FALSE(privacy_sandbox_service->IsHoldingHandle());

  // Navigate again to a valid notice page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav do not queue or hold the handle. Suppress should be true.
  ASSERT_FALSE(privacy_sandbox_service->IsNoticeQueued());
  ASSERT_FALSE(privacy_sandbox_service->IsHoldingHandle());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.QueueOnStartup"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "NoticeQueue.PrivacySandboxNotice.ReleaseOnDMA"),
            1);
}
