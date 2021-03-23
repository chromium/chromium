// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

using DismissalCause =
    javascript_dialogs::TabModalDialogManager::DismissalCause;

class JavaScriptDialogTest : public InProcessBrowserTest {
 private:
  friend class JavaScriptDialogDismissalCauseTester;
};

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, ReloadDoesntHang) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  javascript_dialogs::TabModalDialogManager* js_helper =
      javascript_dialogs::TabModalDialogManager::FromWebContents(tab);

  // Show a dialog.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  js_helper->SetDialogShownCallbackForTesting(runner->QuitClosure());
  tab->GetMainFrame()->ExecuteJavaScriptForTests(u"alert()",
                                                 base::NullCallback());
  runner->Run();

  // Try reloading.
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // If the WaitForLoadStop doesn't hang forever, we've passed.
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       ClosingPageSharingRendererDoesntHang) {
  // Turn off popup blocking.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      embedder_support::kDisablePopupBlocking);

  // Two tabs, one render process.
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsAddedObserver new_wc_observer;
  tab1->GetMainFrame()->ExecuteJavaScriptForTests(
      u"window.open('about:blank');", base::NullCallback());
  content::WebContents* tab2 = new_wc_observer.GetWebContents();
  ASSERT_NE(tab1, tab2);
  ASSERT_EQ(tab1->GetMainFrame()->GetProcess(),
            tab2->GetMainFrame()->GetProcess());

  // Tab two shows a dialog.
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  javascript_dialogs::TabModalDialogManager* js_helper2 =
      javascript_dialogs::TabModalDialogManager::FromWebContents(tab2);
  js_helper2->SetDialogShownCallbackForTesting(runner->QuitClosure());
  tab2->GetMainFrame()->ExecuteJavaScriptForTests(u"alert()",
                                                  base::NullCallback());
  runner->Run();

  // Tab two is closed while the dialog is up.
  int tab2_index = browser()->tab_strip_model()->GetIndexOfWebContents(tab2);
  browser()->tab_strip_model()->CloseWebContentsAt(tab2_index,
                                                   TabStripModel::CLOSE_NONE);

  // Try reloading tab one.
  tab1->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab1));

  // If the WaitForLoadStop doesn't hang forever, we've passed.
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       ClosingPageWithSubframeAlertingDoesntCrash) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  javascript_dialogs::TabModalDialogManager* js_helper =
      javascript_dialogs::TabModalDialogManager::FromWebContents(tab);

  // A subframe shows a dialog.
  std::string dialog_url = "data:text/html,<script>alert(\"hi\");</script>";
  std::string script = "var iframe = document.createElement('iframe');"
                       "iframe.src = '" + dialog_url + "';"
                       "document.body.appendChild(iframe);";
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  js_helper->SetDialogShownCallbackForTesting(runner->QuitClosure());
  tab->GetMainFrame()->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script),
                                                 base::NullCallback());
  runner->Run();

  // The tab is closed while the dialog is up.
  int tab_index = browser()->tab_strip_model()->GetIndexOfWebContents(tab);
  browser()->tab_strip_model()->CloseWebContentsAt(tab_index,
                                                   TabStripModel::CLOSE_NONE);

  // No crash is good news.
}

class JavaScriptCallbackHelper {
 public:
  javascript_dialogs::TabModalDialogManager::DialogClosedCallback
  GetCallback() {
    return base::BindOnce(&JavaScriptCallbackHelper::DialogClosed,
                          base::Unretained(this));
  }

  bool last_success() { return last_success_; }
  std::u16string last_input() { return last_input_; }

 private:
  void DialogClosed(bool success, const std::u16string& user_input) {
    last_success_ = success;
    last_input_ = user_input;
  }

  bool last_success_;
  std::u16string last_input_;
};

// Tests to make sure HandleJavaScriptDialog works correctly.
IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, HandleJavaScriptDialog) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* frame = tab->GetMainFrame();
  javascript_dialogs::TabModalDialogManager* js_helper =
      javascript_dialogs::TabModalDialogManager::FromWebContents(tab);

  JavaScriptCallbackHelper callback_helper;

  // alert
  bool did_suppress = false;
  js_helper->RunJavaScriptDialog(
      tab, frame, content::JAVASCRIPT_DIALOG_TYPE_ALERT, std::u16string(),
      std::u16string(), callback_helper.GetCallback(), &did_suppress);
  ASSERT_TRUE(js_helper->IsShowingDialogForTesting());
  js_helper->HandleJavaScriptDialog(tab, true, nullptr);
  ASSERT_FALSE(js_helper->IsShowingDialogForTesting());
  ASSERT_TRUE(callback_helper.last_success());
  ASSERT_EQ(std::u16string(), callback_helper.last_input());

  // confirm
  for (auto response : {true, false}) {
    js_helper->RunJavaScriptDialog(
        tab, frame, content::JAVASCRIPT_DIALOG_TYPE_CONFIRM, std::u16string(),
        std::u16string(), callback_helper.GetCallback(), &did_suppress);
    ASSERT_TRUE(js_helper->IsShowingDialogForTesting());
    js_helper->HandleJavaScriptDialog(tab, response, nullptr);
    ASSERT_FALSE(js_helper->IsShowingDialogForTesting());
    ASSERT_EQ(response, callback_helper.last_success());
    ASSERT_EQ(std::u16string(), callback_helper.last_input());
  }

  // prompt, cancel
  js_helper->RunJavaScriptDialog(
      tab, frame, content::JAVASCRIPT_DIALOG_TYPE_PROMPT, u"Label",
      std::u16string(), callback_helper.GetCallback(), &did_suppress);
  ASSERT_TRUE(js_helper->IsShowingDialogForTesting());
  js_helper->HandleJavaScriptDialog(tab, false, nullptr);
  ASSERT_FALSE(js_helper->IsShowingDialogForTesting());
  ASSERT_FALSE(callback_helper.last_success());
  ASSERT_EQ(std::u16string(), callback_helper.last_input());

  std::u16string value1 = u"abc";
  std::u16string value2 = u"123";

  // prompt, ok + override
  js_helper->RunJavaScriptDialog(
      tab, frame, content::JAVASCRIPT_DIALOG_TYPE_PROMPT, u"Label", value1,
      callback_helper.GetCallback(), &did_suppress);
  ASSERT_TRUE(js_helper->IsShowingDialogForTesting());
  js_helper->HandleJavaScriptDialog(tab, true, &value2);
  ASSERT_FALSE(js_helper->IsShowingDialogForTesting());
  ASSERT_TRUE(callback_helper.last_success());
  ASSERT_EQ(value2, callback_helper.last_input());

  // prompt, ok + no override
  js_helper->RunJavaScriptDialog(
      tab, frame, content::JAVASCRIPT_DIALOG_TYPE_PROMPT, u"Label", value1,
      callback_helper.GetCallback(), &did_suppress);
  ASSERT_TRUE(js_helper->IsShowingDialogForTesting());
  js_helper->HandleJavaScriptDialog(tab, true, nullptr);
  ASSERT_FALSE(js_helper->IsShowingDialogForTesting());
  ASSERT_TRUE(callback_helper.last_success());
  ASSERT_EQ(value1, callback_helper.last_input());
}

class JavaScriptDialogDismissalCauseTester {
 public:
  explicit JavaScriptDialogDismissalCauseTester(JavaScriptDialogTest* test)
      : tab_(test->browser()->tab_strip_model()->GetActiveWebContents()),
        frame_(tab_->GetMainFrame()),
        js_helper_(
            javascript_dialogs::TabModalDialogManager::FromWebContents(tab_)) {
    js_helper_->SetDialogDismissedCallbackForTesting(base::BindOnce(
        &JavaScriptDialogDismissalCauseTester::SetLastDismissalCause,
        weak_factory_.GetWeakPtr()));
  }

  void PopupDialog(content::JavaScriptDialogType type) {
    bool did_suppress = false;
    js_helper_->RunJavaScriptDialog(tab_, frame_, type, u"Label", u"abc", {},
                                    &did_suppress);
  }

  void ClickDialogButton(bool accept, const std::u16string& user_input) {
    EXPECT_TRUE(js_helper_->IsShowingDialogForTesting());
    js_helper_->ClickDialogButtonForTesting(accept, user_input);
  }

  void Reload() {
    tab_->GetController().Reload(content::ReloadType::NORMAL, false);
    EXPECT_TRUE(content::WaitForLoadStop(tab_));
  }

  void CallHandleDialog(bool accept, const std::u16string* prompt_override) {
    EXPECT_TRUE(js_helper_->IsShowingDialogForTesting());
    js_helper_->HandleJavaScriptDialog(tab_, accept, prompt_override);
  }

  void CallCancelDialogs(bool reset_state) {
    EXPECT_TRUE(js_helper_->IsShowingDialogForTesting());
    js_helper_->CancelDialogs(tab_, reset_state);
  }

  base::Optional<DismissalCause> GetLastDismissalCause() {
    return dismissal_cause_;
  }

  void SetLastDismissalCause(DismissalCause cause) { dismissal_cause_ = cause; }

 private:
  content::WebContents* tab_;
  content::RenderFrameHost* frame_;
  javascript_dialogs::TabModalDialogManager* js_helper_;

  base::Optional<DismissalCause> dismissal_cause_;

  base::WeakPtrFactory<JavaScriptDialogDismissalCauseTester> weak_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, DismissalCausePromptAcceptButton) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  tester.ClickDialogButton(true, std::u16string());
  EXPECT_EQ(DismissalCause::kDialogButtonClicked,
            tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, DismissalCausePromptCancelButton) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  tester.ClickDialogButton(false, std::u16string());
  EXPECT_EQ(DismissalCause::kDialogButtonClicked,
            tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, DismissalCausePromptHandleDialog) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  tester.CallHandleDialog(true, nullptr);
  EXPECT_EQ(DismissalCause::kHandleDialogCalled,
            tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       DismissalCausePromptCancelDialogs) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  tester.CallCancelDialogs(false);
  EXPECT_EQ(DismissalCause::kCancelDialogsCalled,
            tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       DismissalCausePromptTabClosedByUser) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  chrome::CloseTab(browser());
// There are differences in the implementations of Views on different platforms
// that cause different dismissal causes.
#if defined(OS_MAC)
  // On MacOS 10.13, |kDialogClosed| is logged, while for other versions
  // |kCancelDialogsCalled| is logged. Expect only one but not both.
  EXPECT_TRUE(tester.GetLastDismissalCause() ==
                  DismissalCause::kCancelDialogsCalled xor
              tester.GetLastDismissalCause() == DismissalCause::kDialogClosed);
#else
  EXPECT_EQ(DismissalCause::kDialogClosed, tester.GetLastDismissalCause());
#endif
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, DismissalCausePromptTabHidden) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  chrome::NewTab(browser());
  EXPECT_EQ(DismissalCause::kTabHidden, tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       DismissalCausePromptBrowserSwitched) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  chrome::NewEmptyWindow(browser()->profile());
  EXPECT_EQ(DismissalCause::kBrowserSwitched, tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, DismissalCausePromptTabNavigated) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  tester.Reload();
  EXPECT_EQ(DismissalCause::kTabNavigated, tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest,
                       DismissalCausePromptSubsequentDialogShown) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_PROMPT);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_ALERT);
  EXPECT_EQ(DismissalCause::kSubsequentDialogShown,
            tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, NoDismissalAlertTabHidden) {
  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_ALERT);
  chrome::NewTab(browser());
  EXPECT_EQ(base::nullopt, tester.GetLastDismissalCause());
}

IN_PROC_BROWSER_TEST_F(JavaScriptDialogTest, DismissalCauseUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url);

  JavaScriptDialogDismissalCauseTester tester(this);
  tester.PopupDialog(content::JAVASCRIPT_DIALOG_TYPE_CONFIRM);
  tester.ClickDialogButton(true, std::u16string());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AbusiveExperienceHeuristic_JavaScriptDialog::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.front(), url);
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AbusiveExperienceHeuristic_JavaScriptDialog::
          kDismissalCauseName,
      static_cast<int64_t>(DismissalCause::kDialogButtonClicked));
}
