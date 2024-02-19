// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chrome/browser/ui/views/external_protocol_dialog_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/button/checkbox.h"

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestAccept) {
  ShowUi(std::string("https://example.test"));
  dialog_->AcceptDialog();
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_TRUE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestAcceptWithChecked) {
  ShowUi(std::string("https://example.test"));
  SetChecked(true);
  dialog_->AcceptDialog();
  EXPECT_EQ(blocked_scheme_, "telnet");
  EXPECT_EQ(blocked_origin_, url::Origin::Create(GURL("https://example.test")));
  EXPECT_EQ(blocked_state_, BlockState::DONT_BLOCK);
  EXPECT_TRUE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::CHECKED_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestAcceptWithUntrustworthyOrigin) {
  ShowUi(std::string("http://example.test"));
  SetChecked(true);  // This will no-opt because http:// is not trustworthy
  dialog_->AcceptDialog();
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_TRUE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::LAUNCH, 1);
}

// Regression test for http://crbug.com/835216. The OS owns the dialog, so it
// may may outlive the WebContents it is attached to.
IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestAcceptAfterCloseTab) {
  ShowUi(std::string("https://example.test"));
  SetChecked(true);  // |remember_| must be true for the segfault to occur.
  browser()->tab_strip_model()->CloseAllTabs();
  dialog_->AcceptDialog();
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestCancel) {
  ShowUi(std::string("https://example.test"));
  dialog_->CancelDialog();
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestCancelWithChecked) {
  ShowUi(std::string("https://example.test"));
  SetChecked(true);
  dialog_->CancelDialog();
  // Cancel() should not enforce the remember checkbox.
  EXPECT_EQ(blocked_state_, BlockState::UNKNOWN);
  EXPECT_FALSE(url_did_launch_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestClose) {
  // Closing the dialog should be the same as canceling, except for histograms.
  ShowUi(std::string("https://example.test"));
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
  ShowUi(std::string("https://example.test"));
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

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, OriginNameTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/empty.html")));
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      content::JsReplace("location.href = $1",
                         embedded_test_server()->GetURL(
                             "b.test", "/server-redirect?ms-calc:"))));
  WaitForLaunchUrl();
  EXPECT_TRUE(url_did_launch_);
  // The url should be the url of the last redirecting server and not of the
  // request initiator
  EXPECT_EQ(launch_url_, "b.test");
}
