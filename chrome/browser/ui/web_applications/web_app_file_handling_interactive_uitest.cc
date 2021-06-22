// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/file_handling_permission_request_dialog_test_api.h"
#include "chrome/browser/ui/web_applications/web_app_file_handling_test_base.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

// Tests the behavior of the permissions dialog that appears when an app is
// first launched as a file handler.
class WebAppFileHandlingPermissionDialogTest
    : public WebAppFileHandlingTestBase {
 public:
  WebAppFileHandlingPermissionDialogTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFileHandlingPermissionUiV2,
         blink::features::kFileHandlingAPI},
        {});
  }

  void SetUpOnMainThread() override {
    WebAppFileHandlingTestBase::SetUpOnMainThread();
    InstallFileHandlingPWA();
    SetFileHandlingPermission(CONTENT_SETTING_ASK);

    EXPECT_FALSE(FileHandlingPermissionRequestDialogTestApi::IsShowing());

    test_file_path_ = NewTestFilePath("txt");
    LaunchWithFiles(app_id(), GetTextFileHandlerActionURL(), {test_file_path_});

    // A dialog is showing now.
    ASSERT_TRUE(FileHandlingPermissionRequestDialogTestApi::IsShowing());

    // The launch consumer isn't triggered while the dialog is showing.
    VerifyPwaDidReceiveFileLaunchParams(false);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::FilePath test_file_path_;
};

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, AllowAlways) {
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/true,
                                                      /*accept=*/true);
  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path_);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetFileHandlingPermission(GetSecureAppURL()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, AllowOnce) {
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/false,
                                                      /*accept=*/true);
  VerifyPwaDidReceiveFileLaunchParams(true, test_file_path_);
  EXPECT_EQ(CONTENT_SETTING_ASK, GetFileHandlingPermission(GetSecureAppURL()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, BlockAlways) {
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/true,
                                                      /*accept=*/false);
  VerifyPwaDidReceiveFileLaunchParams(false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetFileHandlingPermission(GetSecureAppURL()));
}

IN_PROC_BROWSER_TEST_F(WebAppFileHandlingPermissionDialogTest, BlockOnce) {
  FileHandlingPermissionRequestDialogTestApi::Resolve(/*checked=*/false,
                                                      /*accept=*/false);
  VerifyPwaDidReceiveFileLaunchParams(false);
  EXPECT_EQ(CONTENT_SETTING_ASK, GetFileHandlingPermission(GetSecureAppURL()));
}

}  // namespace web_app
