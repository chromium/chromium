// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_BROWSERTEST_H_

#include "chrome/test/base/web_ui_mocha_browser_test.h"

// |WebUIMochaBrowserTest| for a non-managed user. Used by
// |CloudUploadDialogTest| to enable the Cloud Upload WebUI by ensuring
// |IsEligibleAndEnabledUploadOfficeToCloud| returns true.
class NonManagedUserWebUIBrowserTest : public WebUIMochaBrowserTest {
 public:
  NonManagedUserWebUIBrowserTest() = default;
  NonManagedUserWebUIBrowserTest(const NonManagedUserWebUIBrowserTest&) =
      delete;
  NonManagedUserWebUIBrowserTest& operator=(
      const NonManagedUserWebUIBrowserTest&) = delete;
  ~NonManagedUserWebUIBrowserTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_BROWSERTEST_H_
