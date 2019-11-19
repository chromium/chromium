// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/button/checkbox.h"
#include "url/gurl.h"

namespace test {

class ExternalProtocolDialogTestApi {
 public:
  explicit ExternalProtocolDialogTestApi(ExternalProtocolDialog* dialog)
      : dialog_(dialog) {}

  void SetCheckBoxSelected(bool checked) {
    dialog_->SetRememberSelectionCheckboxCheckedForTesting(checked);
  }

 private:
  ExternalProtocolDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialogTestApi);
};

}  // namespace test

class ExternalProtocolDialogBrowserTest
    : public DialogBrowserTest,
      public ExternalProtocolHandler::Delegate {
 public:
  using BlockState = ExternalProtocolHandler::BlockState;

  ExternalProtocolDialogBrowserTest() {
    ExternalProtocolHandler::SetDelegateForTesting(this);
  }

  ~ExternalProtocolDialogBrowserTest() override {
    ExternalProtocolHandler::SetDelegateForTesting(nullptr);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    dialog_ = new ExternalProtocolDialog(web_contents, GURL("telnet://12345"),
                                         base::UTF8ToUTF16("/usr/bin/telnet"),
                                         base::nullopt);
  }

  void SetChecked(bool checked) {
    test::ExternalProtocolDialogTestApi(dialog_).SetCheckBoxSelected(checked);
  }

  // ExternalProtocolHander::Delegate:
  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol) override {
    return nullptr;
  }
  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::DONT_BLOCK;
  }
  void BlockRequest() override {}
  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin) override {}
  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    url_did_launch_ = true;
  }
  void FinishedProcessingCheck() override {}
  void OnSetBlockState(const std::string& scheme, BlockState state) override {
    blocked_scheme_ = scheme;
    blocked_state_ = state;
  }

  base::HistogramTester histogram_tester_;

 protected:
  ExternalProtocolDialog* dialog_ = nullptr;
  std::string blocked_scheme_;
  BlockState blocked_state_ = BlockState::UNKNOWN;
  bool url_did_launch_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialogBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestAccept) {
  ShowUi(std::string());
  EXPECT_TRUE(dialog_->Accept());
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_TRUE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestAcceptWithChecked) {
  ShowUi(std::string());
  SetChecked(true);
  EXPECT_TRUE(dialog_->Accept());
  EXPECT_EQ(blocked_scheme_, "telnet");
  EXPECT_EQ(blocked_state_, BlockState::DONT_BLOCK);
  EXPECT_TRUE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::CHECKED_LAUNCH, 1);
}

// Regression test for http://crbug.com/835216. The OS owns the dialog, so it
// may may outlive the WebContents it is attached to.
IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestAcceptAfterCloseTab) {
  ShowUi(std::string());
  SetChecked(true);  // |remember_| must be true for the segfault to occur.
  browser()->tab_strip_model()->CloseAllTabs();
  EXPECT_TRUE(dialog_->Accept());
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestCancel) {
  ShowUi(std::string());
  EXPECT_TRUE(dialog_->Cancel());
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestCancelWithChecked) {
  ShowUi(std::string());
  SetChecked(true);
  EXPECT_TRUE(dialog_->Cancel());
  // Cancel() should not enforce the remember checkbox.
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestClose) {
  // Closing the dialog should be the same as canceling, except for histograms.
  ShowUi(std::string());
  EXPECT_TRUE(dialog_->Close());
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestCloseWithChecked) {
  // Closing the dialog should be the same as canceling, except for histograms.
  ShowUi(std::string());
  SetChecked(true);
  EXPECT_TRUE(dialog_->Close());
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

// Invokes a dialog that asks the user if an external application is allowed to
// run.
IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Tests that keyboard focus works when the dialog is shown. Regression test for
// https://crbug.com/1025343.
IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestFocus) {
  ShowUi(std::string());
  gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  const views::FocusManager* focus_manager = widget->GetFocusManager();
  const views::View* focused_view = focus_manager->GetFocusedView();
  EXPECT_TRUE(focused_view);
}
