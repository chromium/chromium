// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_TEST_HARNESS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_TEST_HARNESS_H_

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "url/gurl.h"

class ExternalProtocolDialogBrowserTest
    : public DialogBrowserTest,
      public ExternalProtocolHandler::Delegate {
 public:
  using BlockState = ExternalProtocolHandler::BlockState;

  ExternalProtocolDialogBrowserTest();
  ExternalProtocolDialogBrowserTest(const ExternalProtocolDialogBrowserTest&) =
      delete;
  ExternalProtocolDialogBrowserTest& operator=(
      const ExternalProtocolDialogBrowserTest&) = delete;
  ~ExternalProtocolDialogBrowserTest() override;

  // DialogBrowserTest:
  void ShowUi(const std::string& initiating_origin) override;

  void SetChecked(bool checked);

  // ExternalProtocolHandler::Delegate:
  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override;

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override;
  void BlockRequest() override {}
  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override;
  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override;
  void FinishedProcessingCheck() override {}
  void OnSetBlockState(const std::string& scheme,
                       const url::Origin& initiating_origin,
                       BlockState state) override;

  void SetUpOnMainThread() override;

  void WaitForLaunchUrl();

  base::HistogramTester histogram_tester_;

 protected:
  raw_ptr<ExternalProtocolDialog, AcrossTasksDanglingUntriaged> dialog_ =
      nullptr;
  std::string blocked_scheme_;
  url::Origin blocked_origin_;
  BlockState blocked_state_ = BlockState::UNKNOWN;
  bool url_did_launch_ = false;
  std::string launch_url_;

 private:
  std::unique_ptr<base::RunLoop> launch_url_run_loop_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_TEST_HARNESS_H_
