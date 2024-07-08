// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_user_files {

namespace {

// Get web contents of chrome://local-files-migration.
content::WebContents* GetDialogWebContents() {
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUILocalFilesMigrationURL);
  EXPECT_TRUE(dialog);
  content::WebUI* webui = dialog->GetWebUIForTest();
  EXPECT_TRUE(webui);
  content::WebContents* web_contents = webui->GetWebContents();
  EXPECT_TRUE(web_contents);
  return web_contents;
}

}  // namespace

class LocalFilesMigrationDialogTest : public InProcessBrowserTest {
 public:
  LocalFilesMigrationDialogTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSkyVault, features::kSkyVaultV2},
        /*disabled_features=*/{});
  }
  LocalFilesMigrationDialogTest(const LocalFilesMigrationDialogTest&) = delete;
  LocalFilesMigrationDialogTest& operator=(
      const LocalFilesMigrationDialogTest&) = delete;
  ~LocalFilesMigrationDialogTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationDialogTest, ShowDialog_Dismiss) {
  EXPECT_FALSE(ash::SystemWebDialogDelegate::FindInstance(
      chrome::kChromeUILocalFilesMigrationURL));

  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUILocalFilesMigrationURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  base::MockCallback<StartMigrationCallback> mock_cb;
  EXPECT_CALL(mock_cb, Run).Times(0);
  ASSERT_TRUE(LocalFilesMigrationDialog::Show(CloudProvider::kOneDrive,
                                              base::TimeDelta(base::Hours(1)),
                                              mock_cb.Get()));

  // Wait for chrome://local-files-migration to load.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
  content::WebContents* web_contents = GetDialogWebContents();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return content::EvalJs(
               web_contents,
               "!!document.querySelector('local-files-migration-dialog')")
        .ExtractBool();
  }));

  // Click the OK button and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "document.querySelector('local-files-migration-dialog')"
                      ".$('#dismiss-button').click()"));
  watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(LocalFilesMigrationDialogTest, ShowDialog_UploadNow) {
  EXPECT_FALSE(ash::SystemWebDialogDelegate::FindInstance(
      chrome::kChromeUILocalFilesMigrationURL));

  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUILocalFilesMigrationURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  base::MockCallback<StartMigrationCallback> mock_cb;
  EXPECT_CALL(mock_cb, Run).Times(1);
  ASSERT_TRUE(LocalFilesMigrationDialog::Show(CloudProvider::kGoogleDrive,
                                              base::TimeDelta(base::Hours(1)),
                                              mock_cb.Get()));

  // Wait for chrome://local-files-migration to load.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
  content::WebContents* web_contents = GetDialogWebContents();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return content::EvalJs(
               web_contents,
               "!!document.querySelector('local-files-migration-dialog')")
        .ExtractBool();
  }));

  // Click the Upload now button and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "document.querySelector('local-files-migration-dialog')"
                      ".$('#upload-now-button').click()"));
  watcher.Wait();
}

}  // namespace policy::local_user_files
