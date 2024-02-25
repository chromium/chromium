// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_BROWSERTEST_H_

#include "chrome/test/base/ash/web_ui_browser_test.h"
#include "chromeos/constants/chromeos_features.h"

// |WebUIBrowserTest| for a non-managed user. Used by |CloudUploadDialogTest| to
// enable the Cloud Upload WebUI by ensuring
// |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
// |IsUploadOfficeToCloudEnabled|.
class NonManagedUserWebUIBrowserTest : public WebUIBrowserTest {
 public:
  NonManagedUserWebUIBrowserTest() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
  }

  NonManagedUserWebUIBrowserTest(const NonManagedUserWebUIBrowserTest&) =
      delete;
  NonManagedUserWebUIBrowserTest& operator=(
      const NonManagedUserWebUIBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_BROWSERTEST_H_
