// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_dialog.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::office_fallback {

content::WebContents* GetWebContentsFromOfficeFallbackDialog() {
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUIOfficeFallbackURL);
  EXPECT_TRUE(dialog);
  content::WebUI* webui = dialog->GetWebUIForTest();
  EXPECT_TRUE(webui);
  content::WebContents* web_contents = webui->GetWebContents();
  EXPECT_TRUE(web_contents);
  return web_contents;
}

// Launch the Office Fallback dialog by calling OfficeFallbackDialog::Show()
// with the arguments provided. Wait for the dialog to open and then grab the
// web contents.
content::WebContents* LaunchOfficeFallbackDialogAndGetWebContents(
    const std::vector<storage::FileSystemURL>& file_urls,
    FallbackReason fallback_reason,
    const std::string& action_id,
    DialogChoiceCallback callback) {
  // Watch for Office Fallback dialog URL chrome://office-fallback.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUIOfficeFallbackURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch Office Fallback dialog.
  base::RunLoop run_loop;
  EXPECT_TRUE(OfficeFallbackDialog::Show(file_urls, fallback_reason, action_id,
                                         std::move(callback)));

  // Wait for Office Fallback dialog to open at chrome://office-fallback.
  navigation_observer_dialog.Wait();
  EXPECT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Get the web contents of the dialog to be able to query
  // `OfficeFallbackElement`.
  return GetWebContentsFromOfficeFallbackDialog();
}

class OfficeFallbackDialogBrowserTest : public InProcessBrowserTest {
 public:
  OfficeFallbackDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         chromeos::features::kUploadOfficeToCloudForEnterprise},
        {});
  }

  OfficeFallbackDialogBrowserTest(const OfficeFallbackDialogBrowserTest&) =
      delete;
  OfficeFallbackDialogBrowserTest& operator=(
      const OfficeFallbackDialogBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    files_ = file_manager::test::CopyTestFilesIntoMyFiles(browser()->profile(),
                                                          {"text.docx"});
  }

 protected:
  std::vector<storage::FileSystemURL> files_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct title is displayed when the
// fallback reason is that the system is offline.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenOffline) {
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContents(
          files_, FallbackReason::kOffline,
          file_manager::file_tasks::kActionIdWebDriveOfficeWord,
          base::DoNothing());

  content::EvalJsResult eval_result =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#title').innerText");
  EXPECT_EQ(eval_result.ExtractString(),
            l10n_util::GetStringFUTF8(
                IDS_OFFICE_FALLBACK_TITLE_OFFLINE,
                files_.front().path().BaseName().LossyDisplayName()));
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct title is displayed when the
// fallback reason is that Drive is unavailable.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenDriveUnavailable) {
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContents(
          files_, FallbackReason::kDriveDisabled,
          file_manager::file_tasks::kActionIdWebDriveOfficeWord,
          base::DoNothing());

  content::EvalJsResult eval_result =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#title').innerText");
  EXPECT_EQ(eval_result.ExtractString(),
            l10n_util::GetStringFUTF8(
                IDS_OFFICE_FALLBACK_TITLE_DRIVE_UNAVAILABLE,
                files_.front().path().BaseName().LossyDisplayName()));
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the cancel button works.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest, ClickCancel) {
  base::RunLoop run_loop;
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContents(
          files_, FallbackReason::kOffline,
          file_manager::file_tasks::kActionIdWebDriveOfficeWord,
          base::BindLambdaForTesting(
              [&run_loop](std::optional<const std::string> choice) {
                // Expect the dialog is closed with the "cancel" user choice.
                if (choice.has_value() &&
                    choice.value() ==
                        ash::office_fallback::kDialogChoiceCancel) {
                  run_loop.Quit();
                }
              }));

  // Click the close button and wait until the dialog is closed with the correct
  // user choice.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('office-fallback')"
                              ".$('#cancel-button').click()"));

  run_loop.Run();
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the try again button works.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest, ClickTryAgain) {
  base::RunLoop run_loop;
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContents(
          files_, FallbackReason::kOffline,
          file_manager::file_tasks::kActionIdWebDriveOfficeWord,
          base::BindLambdaForTesting(
              [&run_loop](std::optional<const std::string> choice) {
                // Expect the dialog is closed with the "cancel" user choice.
                if (choice.has_value() &&
                    choice.value() ==
                        ash::office_fallback::kDialogChoiceTryAgain) {
                  run_loop.Quit();
                }
              }));

  // Click the try again button and wait until the dialog is closed with the
  // correct user choice.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('office-fallback')"
                              ".$('#try-again-button').click()"));

  run_loop.Run();
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the quick office button works.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest, ClickQuickOffice) {
  base::RunLoop run_loop;
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContents(
          files_, FallbackReason::kOffline,
          file_manager::file_tasks::kActionIdWebDriveOfficeWord,
          base::BindLambdaForTesting(
              [&run_loop](std::optional<const std::string> choice) {
                // Expect the dialog is closed with the "cancel" user choice.
                if (choice.has_value() &&
                    choice.value() ==
                        ash::office_fallback::kDialogChoiceQuickOffice) {
                  run_loop.Quit();
                }
              }));

  // Click the try again button and wait until the dialog is closed with the
  // correct user choice.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('office-fallback')"
                              ".$('#quick-office-button').click()"));

  run_loop.Run();
}

}  // namespace ash::office_fallback
