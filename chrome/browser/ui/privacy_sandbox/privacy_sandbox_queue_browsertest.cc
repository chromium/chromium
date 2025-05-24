// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_queue_manager.h"
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
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/user_education/test/test_product_messaging_controller.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kNoticeId);

// This file is meant to test the general code path that causes notices to show.
// Specifically triggering of the notice queue.
class PrivacySandboxQueueTestNotice : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kPrivacySandboxNoticeQueue}, {});
  }

  void SetUpOnMainThread() override {
    privacy_sandbox_service()->SetPromptDisabledForTests(false);
    privacy_sandbox_service()->ForceChromeBuildForTests(true);
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "be");
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(switches::kNoFirstRun);
  }

  content::WebContents* web_contents(PrivacySandboxDialogView* dialog_widget) {
    return dialog_widget->GetWebContentsForTesting();
  }

  privacy_sandbox::PrivacySandboxQueueManager& queue_manager() {
    return privacy_sandbox_service()->GetPrivacySandboxNoticeQueueManager();
  }

  PrivacySandboxService* privacy_sandbox_service() {
    return PrivacySandboxServiceFactory::GetForProfile(browser()->profile());
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
  // Navigate to invalid page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // When we navigate to a page that doesn't require a prompt, we should still
  // hog handle.
  ASSERT_TRUE(queue_manager().IsHoldingHandle());

  // Navigate to valid page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_TRUE(queue_manager().IsHoldingHandle());
}

// Navigate to a valid webpage (settings page) and click a notice. One window.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice, PromptShows) {
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
  ASSERT_TRUE(queue_manager().IsHoldingHandle());

  // Click ack button.
  const std::string& code = element_path_ + ".click();";
  EXPECT_TRUE(content::ExecJs(web_contents(dialog_widget), code,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              1 /* world_id */));

  // After click, should release handle.
  ASSERT_FALSE(queue_manager().IsHoldingHandle());
}

// Navigate to a valid webpage (settings page) and click a notice. Two windows.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice,
                       PromptShowsMultipleWindows) {
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
  ASSERT_TRUE(queue_manager().IsHoldingHandle());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav, should still be holding handle.
  ASSERT_TRUE(queue_manager().IsHoldingHandle());

  // Click ack button on one window.
  const std::string& code = element_path_ + ".click();";
  EXPECT_TRUE(content::ExecJs(web_contents(dialog_widget), code,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              1 /* world_id */));

  // After click, should release handle.
  ASSERT_FALSE(queue_manager().IsHoldingHandle());
}

// Browser startup assumes we don't need a notice. Then we need a notice.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice,
                       PromptNeededAtStartupThenNotAtNavigation) {
  // Set flags incorrectly so we don't need a prompt.
  privacy_sandbox_service()->SetPromptDisabledForTests(true);

  // Navigate.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After first nav, we should not be holding handle.
  ASSERT_FALSE(queue_manager().IsHoldingHandle());
  ASSERT_FALSE(queue_manager().IsNoticeQueued());

  // Set the correct flag.
  privacy_sandbox_service()->SetPromptDisabledForTests(false);

  // Navigate again and now we want to queue and hold the handle.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav, should have been queued and holding handle.
  ASSERT_TRUE(queue_manager().IsHoldingHandle());
}

// Browser startup assumes we need a notice. Then we realize we don't need it.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice,
                       PromptNotNeededAtStartupThenNeededAtNavigation) {
  // Navigate.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After first trigger, should have been queued and holding handle.
  ASSERT_TRUE(queue_manager().IsHoldingHandle());

  // Change our mind about wanting a prompt.
  privacy_sandbox_service()->SetPromptDisabledForTests(true);

  // Navigate.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav do not hold handle.
  ASSERT_FALSE(queue_manager().IsNoticeQueued());
  ASSERT_FALSE(queue_manager().IsHoldingHandle());
}

// Don't allow the notice to be queued, such that the handle check fails, and we
// can log a QueueState.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice,
                       TestNoticeQueueStateNotInQueue) {
  base::HistogramTester histogram_tester;
  // Suppress attempts to queue.
  queue_manager().SetSuppressQueue(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_FALSE(histogram_tester
                   .GetTotalCountsForPrefix(
                       "PrivacySandbox.Notice.NotHoldingHandle.NotInQueue")
                   .empty());
}

// Allow the notice to be queued, but add a blocking notice that doesn't allow
// our spot in the queue to trigger. The handle check repeatedly fails, and we
// are still in the queue.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNotice,
                       TestNoticeQueueStateStuckInQueue) {
  base::HistogramTester histogram_tester;

  // Add a test notice before our notice that hogs the handle.
  user_education::test::TestNotice notice(
      *queue_manager().GetProductMessagingController(), kNoticeId);

  // Navigate to valid page, failing the holdingHandle check.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_FALSE(histogram_tester
                   .GetTotalCountsForPrefix(
                       "PrivacySandbox.Notice.NotHoldingHandle.InQueue")
                   .empty());
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

  void SetUpOnMainThread() override {
    PrivacySandboxQueueTestNotice::SetUpOnMainThread();
    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);
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
  // When we navigate to valid page for SE dialog, we should unqueue.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_FALSE(queue_manager().IsHoldingHandle());

  // Navigate again to a valid notice page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // After second nav do not queue or hold the handle.
  ASSERT_FALSE(queue_manager().IsNoticeQueued());
  ASSERT_FALSE(queue_manager().IsHoldingHandle());
}

class PrivacySandboxQueueTestNoticeFeatureDisabled
    : public PrivacySandboxQueueTestNotice {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitWithFeatures(
        {}, {privacy_sandbox::kPrivacySandboxNoticeQueue});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Navigate to a page and click a button.
IN_PROC_BROWSER_TEST_F(PrivacySandboxQueueTestNoticeFeatureDisabled,
                       ShowAndClickPrompt) {
  // Should not be queued after browser startup
  ASSERT_FALSE(queue_manager().IsNoticeQueued());
  ASSERT_FALSE(queue_manager().IsHoldingHandle());

  // Navigate
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
  ASSERT_FALSE(queue_manager().IsNoticeQueued());
  ASSERT_FALSE(queue_manager().IsHoldingHandle());

  // Click ack button.
  const std::string& code = element_path_ + ".click();";
  EXPECT_TRUE(content::ExecJs(web_contents(dialog_widget), code,
                              content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                              1 /* world_id */));

  // After click, should release handle.
  ASSERT_FALSE(queue_manager().IsNoticeQueued());
  ASSERT_FALSE(queue_manager().IsHoldingHandle());
}
