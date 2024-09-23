// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_dialog.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::office_fallback {

namespace {
static const char kTaskTitle[] = "some app";
}  // namespace

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

// Launch the Office Fallback dialog at chrome://office-fallback by calling
// OfficeFallbackDialog::Show() with the arguments provided. Wait until the
// "office-fallback" DOM element exists at chrome://office-fallback and return
// the web contents.
content::WebContents* LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
    const std::vector<storage::FileSystemURL>& file_urls,
    FallbackReason fallback_reason,
    const std::string& task_title,
    DialogChoiceCallback callback) {
  // Watch for Office Fallback dialog URL chrome://office-fallback.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUIOfficeFallbackURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch Office Fallback dialog.
  EXPECT_TRUE(OfficeFallbackDialog::Show(file_urls, fallback_reason, task_title,
                                         std::move(callback)));

  // Wait for chrome://office-fallback to open.
  navigation_observer_dialog.Wait();
  EXPECT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Get the web contents of the dialog to be able to check that it is exists.
  content::WebContents* web_contents = GetWebContentsFromOfficeFallbackDialog();

  // Wait until the DOM element actually exists at office-fallback.
  EXPECT_TRUE(base::test::RunUntil([&] {
    return content::EvalJs(web_contents,
                           "!!document.querySelector('office-fallback')")
        .ExtractBool();
  }));

  return web_contents;
}

class OfficeFallbackDialogBrowserTest : public InProcessBrowserTest {
 public:
  OfficeFallbackDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         chromeos::features::kMicrosoftOneDriveIntegrationForEnterprise,
         chromeos::features::kUploadOfficeToCloudForEnterprise},
        {});
  }

  OfficeFallbackDialogBrowserTest(const OfficeFallbackDialogBrowserTest&) =
      delete;
  OfficeFallbackDialogBrowserTest& operator=(
      const OfficeFallbackDialogBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // This is needed to simulate the presence of the ODFS extension, which is
    // checked in `IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled`.
    auto fake_provider =
        ash::file_system_provider::FakeExtensionProvider::Create(
            extension_misc::kODFSExtensionId);
    auto* service =
        ash::file_system_provider::Service::Get(browser()->profile());
    service->RegisterProvider(std::move(fake_provider));

    files_ = file_manager::test::CopyTestFilesIntoMyFiles(browser()->profile(),
                                                          {"text.docx"});
  }

 protected:
  std::vector<storage::FileSystemURL> files_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct title and reason is displayed
// when the fallback reason is that the system is offline.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenOffline) {
  // Launch Office Fallback dialog.
  const std::string application_name = "Test app name";
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kOffline, application_name,
          base::DoNothing());

  content::EvalJsResult eval_result_title =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#title').innerText");
  EXPECT_EQ(eval_result_title.ExtractString(),
            l10n_util::GetStringFUTF8(
                IDS_OFFICE_FALLBACK_TITLE_OFFLINE,
                files_.front().path().BaseName().LossyDisplayName()));

  content::EvalJsResult eval_result_reason =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#reason-message').innerText");
  EXPECT_EQ(eval_result_reason.ExtractString(),
            l10n_util::GetStringFUTF8(IDS_OFFICE_FALLBACK_REASON_OFFLINE,
                                      base::UTF8ToUTF16(application_name)));
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct title is displayed when the
// fallback reason is that Drive authentication is not ready.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenDriveAuthenticationNotReady) {
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kDriveAuthenticationNotReady, kTaskTitle,
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
// `OfficeFallbackElement`. Tests that the correct instructions are displayed
// when the fallback reason is that the disable Drive preference is set.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenDisableDrivePreferenceSet) {
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kDisableDrivePreferenceSet, kTaskTitle,
          base::DoNothing());

  content::EvalJsResult eval_result =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#instructions-message').innerText");
  EXPECT_EQ(eval_result.ExtractString(),
            l10n_util::GetStringUTF8(
                IDS_OFFICE_FALLBACK_INSTRUCTIONS_DISABLE_DRIVE_PREFERENCE));
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct reason and instructions are
// displayed when the fallback reason is that Drive is unavailable for the
// account type.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenDriveDisabledForAccountType) {
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kDriveDisabledForAccountType, kTaskTitle,
          base::DoNothing());

  content::EvalJsResult eval_result_instructions =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#instructions-message').innerText");
  EXPECT_EQ(eval_result_instructions.ExtractString(),
            l10n_util::GetStringUTF8(
                IDS_OFFICE_FALLBACK_INSTRUCTIONS_DRIVE_DISABLED_FOR_ACCOUNT));

  content::EvalJsResult eval_result_reason =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#reason-message').innerText");
  EXPECT_EQ(eval_result_reason.ExtractString(),
            l10n_util::GetStringUTF8(
                IDS_OFFICE_FALLBACK_REASON_DRIVE_DISABLED_FOR_ACCOUNT));
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct instructions are displayed
// when the fallback reason is that Drive has not service.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenNoDriveService) {
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kNoDriveService, kTaskTitle,
          base::DoNothing());

  content::EvalJsResult eval_result =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#instructions-message').innerText");
  EXPECT_EQ(eval_result.ExtractString(),
            l10n_util::GetStringUTF8(IDS_OFFICE_FALLBACK_INSTRUCTIONS));
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct instructions are displayed
// when the fallback reason is that the file is still waiting to be uploaded,
// and that the correct user choice is received after clicking the OK button.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest,
                       OfficeFallbackDialogWhenWaitingForUpload) {
  base::RunLoop run_loop;
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kWaitingForUpload, kTaskTitle,
          base::BindLambdaForTesting(
              [&run_loop](std::optional<const std::string> choice) {
                // Expect the dialog is closed with the "cancel" user choice.
                if (choice.has_value() &&
                    choice.value() == ash::office_fallback::kDialogChoiceOk) {
                  run_loop.Quit();
                }
              }));

  // Check the displayed instruction.
  content::EvalJsResult eval_result =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#instructions-message').innerText");
  EXPECT_EQ(eval_result.ExtractString(),
            l10n_util::GetStringUTF8(
                IDS_OFFICE_FALLBACK_INSTRUCTIONS_WAITING_FOR_UPLOAD));

  // Click the OK button and wait until the dialog is closed with the correct
  // user choice.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('office-fallback')"
                              ".$('#ok-button').click()"));
  run_loop.Run();
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the correct instructions are displayed
// when the fallback reason is that the file cannot be open from its current
// Android OneDrive location, and that the correct user choice is received after
// clicking the OK button.
IN_PROC_BROWSER_TEST_F(
    OfficeFallbackDialogBrowserTest,
    OfficeFallbackDialogWhenAndroidOneDriveLocationNotSupported) {
  base::RunLoop run_loop;
  // Launch Office Fallback dialog.
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kAndroidOneDriveUnsupportedLocation,
          "Microsoft 365",
          base::BindLambdaForTesting(
              [&run_loop](std::optional<const std::string> choice) {
                // Expect the dialog is closed with the "OK" user choice.
                if (choice.has_value() &&
                    choice.value() == ash::office_fallback::kDialogChoiceOk) {
                  run_loop.Quit();
                }
              }));

  // Check the displayed instruction.
  content::EvalJsResult eval_result =
      content::EvalJs(web_contents,
                      "document.querySelector('office-fallback')"
                      ".$('#instructions-message').innerText");
  EXPECT_EQ(
      eval_result.ExtractString(),
      l10n_util::GetStringUTF8(
          IDS_OFFICE_FALLBACK_INSTRUCTIONS_ANDROID_ONE_DRIVE_LOCATION_NOT_SUPPORTED));

  // Click the OK button and wait until the dialog is closed with the correct
  // user choice.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('office-fallback')"
                              ".$('#ok-button').click()"));
  run_loop.Run();
}

// Test which launches an `OfficeFallbackDialog` which in turn creates an
// `OfficeFallbackElement`. Tests that the cancel button works.
IN_PROC_BROWSER_TEST_F(OfficeFallbackDialogBrowserTest, ClickCancel) {
  base::RunLoop run_loop;
  content::WebContents* web_contents =
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kOffline, kTaskTitle,
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
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kOffline, kTaskTitle,
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
      LaunchOfficeFallbackDialogAndGetWebContentsForDialog(
          files_, FallbackReason::kOffline, kTaskTitle,
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
