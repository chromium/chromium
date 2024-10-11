// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog_browsertest.h"

#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_parser.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/volume_manager.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

namespace ash::cloud_upload {

namespace {

namespace fm_tasks = ::file_manager::file_tasks;

using chromeos::cloud_upload::kCloudUploadPolicyAllowed;
using chromeos::cloud_upload::kCloudUploadPolicyAutomated;
using chromeos::cloud_upload::kCloudUploadPolicyDisallowed;

// The mime type and file_extension must be matching for
// `CreateFakeWebApps()`.
const char kDocFileExtension[] = ".doc";
const char kDocMimeType[] = "application/msword";
const char kDocxFileExtension[] = ".docx";
const char kDocxMimeType[] =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
const char kPptFileExtension[] = ".ppt";
const char kPptMimeType[] = "application/vnd.ms-powerpoint";
const char kPptxFileExtension[] = ".pptx";
const char kPptxMimeType[] =
    "application/vnd.openxmlformats-officedocument.presentationml.presentation";
const char kXlsxFileExtension[] = ".xlsx";
const char kXlsxMimeType[] =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";

// Create `n` fake web apps that open any files with the `file_extensions` and
// matching `mime_types`. The apps can be identified by a unique `app_id`
// and launched by file_manager::file_tasks::ExecuteFileTask() which can be
// observed by the unique `url`. Store the info of the created tasks in `urls`
// and `tasks`.
void CreateFakeWebApps(
    Profile* profile,
    std::vector<std::string>* urls,
    std::vector<file_manager::file_tasks::TaskDescriptor>* tasks,
    const std::vector<std::string>& file_extensions,
    const std::vector<std::string>& mime_types,
    int n) {
  ASSERT_EQ(file_extensions.size(), mime_types.size());
  for (int i = 0; i < n; ++i) {
    std::string start_url =
        "https://www.example" + base::NumberToString(i) + ".com";
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL(start_url));
    web_app_info->scope = GURL(start_url);
    apps::FileHandler handler;
    std::string url = start_url + "/handle_file";
    handler.action = GURL(url);
    handler.display_name = u"activity name";
    for (size_t j = 0; j < file_extensions.size(); j++) {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.file_extensions.insert(file_extensions[j]);
      accept_entry.mime_type = mime_types[j];
      handler.accept.push_back(accept_entry);
    }
    web_app_info->file_handlers.push_back(std::move(handler));

    // Install a PWA in ash.
    std::string app_id =
        web_app::test::InstallWebApp(profile, std::move(web_app_info));
    // Skip past the permission dialog.
    web_app::WebAppProvider::GetForTest(profile)
        ->sync_bridge_unsafe()
        .SetAppFileHandlerApprovalState(app_id,
                                        web_app::ApiApprovalState::kAllowed);

    file_manager::file_tasks::TaskDescriptor task_descriptor(
        app_id, file_manager::file_tasks::TaskType::TASK_TYPE_WEB_APP, url);

    urls->push_back(url);
    tasks->push_back(task_descriptor);
  }
}

// Returns the position of `elt` in `list`. If `list` does not contain `elt`,
// return -1.
int PositionInList(base::Value::List& list, const std::string& elt) {
  for (size_t i = 0; i < list.size(); i++) {
    if (elt == list[i]) {
      return i;
    }
  }
  return -1;
}

// Get web contents of chrome://cloud-upload.
content::WebContents* GetWebContentsFromCloudUploadDialog() {
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUICloudUploadURL);
  EXPECT_TRUE(dialog);
  content::WebUI* webui = dialog->GetWebUIForTest();
  EXPECT_TRUE(webui);
  content::WebContents* web_contents = webui->GetWebContents();
  EXPECT_TRUE(web_contents);
  return web_contents;
}

// Call `CloudOpenTask::Execute()` with the arguments provided and expect that
// dialog will appear at chrome://cloud-upload. Wait until chrome://cloud-upload
// opens.
void LaunchCloudUploadDialog(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const CloudProvider cloud_provider,
    std::unique_ptr<CloudOpenMetrics> cloud_open_metrics) {
  // Watch for dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch dialog.
  EXPECT_TRUE(CloudOpenTask::Execute(
      profile, file_urls, file_manager::file_tasks::TaskDescriptor(),
      cloud_provider, std::move(cloud_open_metrics)));

  // Wait for chrome://cloud-upload to open.
  navigation_observer_dialog.Wait();
  EXPECT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
}

// Wait until the provided JS `script` returns true when executed on the
// `web_contents`.
void WaitUntilJsReturnsTrue(content::WebContents* web_contents,
                            std::string script) {
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return content::EvalJs(web_contents, script).ExtractBool(); }));
}

// Wait until the `document` element with `element_name` exists on the
// `web_contents`.
void WaitUntilElementExists(content::WebContents* web_contents,
                            std::string element_name) {
  WaitUntilJsReturnsTrue(web_contents,
                         "!!document.querySelector('" + element_name + "')");
}

// Expect that calling `CloudOpenTask::Execute()` with the provided arguments
// will launch a cloud upload dialog at chrome://cloud-upload. Wait until the
// DOM element with `dialog_name` exists at chrome://cloud-upload and return the
// web contents at chrome://cloud-upload.
content::WebContents* LaunchCloudUploadDialogAndGetWebContentsForDialog(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const CloudProvider cloud_provider,
    std::unique_ptr<CloudOpenMetrics> cloud_open_metrics,
    std::string dialog_name) {
  LaunchCloudUploadDialog(profile, file_urls, cloud_provider,
                          std::move(cloud_open_metrics));

  // Get the web contents of chrome://cloud-upload to be able to check that the
  // dialog exists.
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Wait until the DOM element actually exists at chrome://cloud-upload.
  WaitUntilElementExists(web_contents, dialog_name);

  return web_contents;
}

// Set email (using a domain from |kNonManagedDomainPatterns|) to login a
// non-managed user. Intended to be used in the override of |SetUpCommandLine|
// from |InProcessBrowserTest| to ensure
// |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
// |IsUploadOfficeToCloudEnabled| in browser tests.
void SetUpCommandLineForNonManagedUser(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kLoginUser, "testuser@gmail.com");
  command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
}

// A matcher to verify that std::optional<TaskDescriptor> corresponds to a Web
// Drive Office Task.
auto IsWebDriveOfficeTask() {
  return testing::Optional(testing::ResultOf(
      &file_manager::file_tasks::IsWebDriveOfficeTask, testing::Eq(true)));
}

// A matcher to verify that std::optional<TaskDescriptor> corresponds to an
// Open in Office Task.
auto IsOpenInOfficeTask() {
  return testing::Optional(testing::ResultOf(
      &file_manager::file_tasks::IsOpenInOfficeTask, testing::Eq(true)));
}

}  // namespace

// Tests the `kFileHandlerDialog` dialog page of the `CloudUploadDialog`.
// Ensures that fake local file tasks are all found and passed to the JS side of
// the dialog - the `FileHandlerPageElement`. Ensures that a local file task
// selected on the JS side gets executed.
class FileHandlerDialogBrowserTest : public InProcessBrowserTest {
 public:
  FileHandlerDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         chromeos::features::kUploadOfficeToCloudForEnterprise},
        {});
  }

  explicit FileHandlerDialogBrowserTest(int num_tasks)
      : FileHandlerDialogBrowserTest() {
    num_tasks_ = num_tasks;
  }

  FileHandlerDialogBrowserTest(const FileHandlerDialogBrowserTest&) = delete;
  FileHandlerDialogBrowserTest& operator=(const FileHandlerDialogBrowserTest&) =
      delete;

  Profile* profile() { return browser()->profile(); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Needed to check that Files app was launched as the dialog's modal parent.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();

    SetUpTasksAndFiles();
  }

  // Create test office files and store in `files_` and create `num_tasks_` fake
  // web apps for all office file types.
  void SetUpTasksAndFiles() {
    // Create `num_tasks_` fake web apps for office files with the Doc extension
    // and store the created `urls_` and `tasks_`.
    CreateFakeWebApps(
        profile(), &urls_, &tasks_,
        {kDocxFileExtension, kPptxFileExtension, kXlsxFileExtension},
        {kDocxMimeType, kPptxMimeType, kXlsxMimeType}, num_tasks_);

    files_ = file_manager::test::CopyTestFilesIntoMyFiles(
        profile(), {"text.docx", "presentation.pptx"});
  }

 protected:
  // Use a non-managed user in this browser test to ensure
  // |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
  // |IsUploadOfficeToCloudEnabled|.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpCommandLineForNonManagedUser(command_line);
  }

  int num_tasks_ = 3;
  std::vector<std::string> urls_;
  std::vector<file_manager::file_tasks::TaskDescriptor> tasks_;
  std::vector<storage::FileSystemURL> files_;
  base::HistogramTester histogram_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a new Files app window is created if no Files app window exists.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, NewModalParentCreated) {
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());

  Browser* browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_EQ(nullptr, browser);

  // Launch File Handler dialog.
  LaunchCloudUploadDialog(
      profile(), files_, CloudProvider::kGoogleDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                         /*file_count=*/1));

  browser = FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_NE(nullptr, browser);
}

// Tests that a new Files app window is not created when there is a Files app
// window already open.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       ExistingWindowUsedAsModalParent) {
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());

  Browser* browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_EQ(nullptr, browser);

  // Open a files app window.
  ui_test_utils::BrowserChangeObserver browser_added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  base::test::TestFuture<platform_util::OpenOperationResult> future;
  file_manager::util::ShowItemInFolder(profile(), files_.at(0).path(),
                                       future.GetCallback());
  EXPECT_EQ(future.Get(), platform_util::OpenOperationResult::OPEN_SUCCEEDED);
  browser_added_observer.Wait();

  browser = FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_NE(nullptr, browser);

  // Launch File Handler dialog.
  LaunchCloudUploadDialog(
      profile(), files_, CloudProvider::kGoogleDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                         /*file_count=*/1));

  // Check that the existing Files app window was used.
  ASSERT_EQ(browser,
            FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER));
}

// Test which launches a `CloudUploadDialog` which in turn creates a
// `FileHandlerPageElement`. Tests that the cancel button works and a Cancel
// TaskResult is logged.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, CancelFileHandlerDialog) {
  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();

  // Check that the Setup flow has never run and so the File Handler dialog will
  // be launched when CloudOpenTask::Execute() is called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog and get the web contents of the dialog to be
  // able to query `FileHandlerPageElement`.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, CloudProvider::kGoogleDrive,
          std::move(cloud_open_metrics), "file-handler-page");

  // Click the close button and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('file-handler-page')"
                              ".$('.cancel-button').click()"));
  watcher.Wait();

  // Expect a kCancelledAtSetup TaskResult.
  histogram_.ExpectUniqueSample(
      ash::cloud_upload::kGoogleDriveTaskResultMetricName,
      ash::cloud_upload::OfficeTaskResult::kCancelledAtSetup, 1);

  // cloud_open_metrics should have been destroyed by the end of the test.
  ASSERT_TRUE(cloud_open_metrics_weak_ptr.WasInvalidated());
}

// Test which launches a `CloudUploadDialog` which in turn creates a
// `FileHandlerPageElement`. Tests that closing the Files app the dialog is
// modal to also closes the dialog and a Cancel TaskResult is logged.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       ClosingFilesAppCancelsDialog) {
  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();

  // Check that the Setup flow has never run and so the File Handler dialog will
  // be launched when CloudOpenTask::Execute() is called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog and get the web contents of the dialog.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, CloudProvider::kGoogleDrive,
          std::move(cloud_open_metrics), "file-handler-page");

  // Close the Files app and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  Browser* files_app_browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  files_app_browser->window()->Close();
  watcher.Wait();

  // Expect a kCancelledAtSetup TaskResult.
  histogram_.ExpectUniqueSample(
      ash::cloud_upload::kGoogleDriveTaskResultMetricName,
      ash::cloud_upload::OfficeTaskResult::kCancelledAtSetup, 1);

  // cloud_open_metrics should have been destroyed by the end of the test.
  ASSERT_TRUE(cloud_open_metrics_weak_ptr.WasInvalidated());
}

// Test which launches a `CloudUploadDialog` which in turn creates a
// `FileHandlerPageElement`. Tests that when the dialog closes unexpectedly, a
// Cancel TaskResult is logged.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, DialogClosedUnexpectedly) {
  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();

  // Check that the Setup flow has never run and so the File Handler dialog will
  // be launched when CloudOpenTask::Execute() is called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog and get the web contents of the dialog.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, CloudProvider::kGoogleDrive,
          std::move(cloud_open_metrics), "file-handler-page");

  // Close the dialog with no user response and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUICloudUploadURL);
  EXPECT_TRUE(dialog);
  dialog->Close();

  watcher.Wait();

  // Expect a kCancelledAtSetup TaskResult.
  histogram_.ExpectUniqueSample(
      ash::cloud_upload::kGoogleDriveTaskResultMetricName,
      ash::cloud_upload::OfficeTaskResult::kCancelledAtSetup, 1);

  // cloud_open_metrics should have been destroyed by the end of the test.
  ASSERT_TRUE(cloud_open_metrics_weak_ptr.WasInvalidated());
}

// Test which launches a `CloudUploadDialog` which in turn creates a
// `FileHandlerPageElement`. Tests that the `FileHandlerPageElement` observes
// all of the fake file tasks and that a file task can be launched by clicking
// on its button before clicking the open button. Tests that the correct
// TaskResult is logged
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, OpenFileTaskFromDialog) {
  // Install QuickOffice.
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());

  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();

  // Check that the Setup flow has never run and so the File Handler dialog will
  // be launched when CloudOpenTask::Execute() is called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog and get the web contents of the dialog to be
  // able to query `FileHandlerPageElement`.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, CloudProvider::kGoogleDrive,
          std::move(cloud_open_metrics), "file-handler-page");

  // Get the `tasks` member from the `FileHandlerPageElement` which are all of
  // the observed local file tasks.
  base::Value::List observed_app_ids;
  ASSERT_TRUE(base::test::RunUntil([&] {
    // It is possible that the `FileHandlerPageElement` element still hasn't
    // been initiated yet. It is completed when the `localTasks` member is
    // non-empty.
    content::EvalJsResult eval_result =
        content::EvalJs(web_contents,
                        "document.querySelector('file-handler-page')"
                        ".localTasks.map(task => task.appId)");
    if (!eval_result.error.empty()) {
      return false;
    }
    observed_app_ids = eval_result.ExtractList().TakeList();
    return !observed_app_ids.empty();
  }));

// Check QuickOffice was observed by the dialog as it should always be shown.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ASSERT_TRUE(file_manager::file_tasks::IsQuickOfficeInstalled(profile()));
  ASSERT_GE(PositionInList(observed_app_ids,
                           extension_misc::kQuickOfficeComponentExtensionId),
            0);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Position (in the `tasks_` and `urls_` vector) of the selected file
  // task to be opened. Use this to find the `selected_task_position` and to
  // watch for the appropriate url in `urls_` to open.
  size_t selected_task = 1;
  // Position of the selected task in dialog's localTasks array - this is not
  // necessarily the same as the `tasks_` vector. Its position is its id
  // so use this to click the task's button.
  size_t selected_task_position;

  // Check that each local file task was observed in the dialog.
  for (size_t i = 0; i < tasks_.size(); i++) {
    int position = PositionInList(observed_app_ids, tasks_[i].app_id);
    ASSERT_GE(position, 0);
    // Record the `selected_task_position`.
    if (i == selected_task) {
      selected_task_position = position;
    }
  }

  // Watch for the selected task to open.
  content::TestNavigationObserver navigation_observer_task(
      (GURL(urls_[selected_task])));
  navigation_observer_task.StartWatchingNewWebContents();

  // Check that there is not a default task for doc files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocxMimeType, kDocxFileExtension));

  // Check that there is not a default task for pptx files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kPptxMimeType, kPptxFileExtension));

  // Expand local tasks accordion.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      "document.querySelector('file-handler-page').$('#accordion').click()"));

  // Click the selected task.
  std::string position_string = base::NumberToString(selected_task_position);
  EXPECT_TRUE(content::ExecJs(
      web_contents, "document.querySelector('file-handler-page').$('#id" +
                        position_string + "').click()"));

  // Click the open button.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('file-handler-page')"
                              ".$('.action-button').click()"));

  // Wait for selected task to open.
  navigation_observer_task.Wait();

  // Check that the Setup flow has been marked complete.
  ASSERT_TRUE(file_manager::file_tasks::HasExplicitDefaultFileHandler(profile(),
                                                                      ".docx"));

  // Check that the selected task has been made the default for doc files.
  ASSERT_EQ(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                *profile()->GetPrefs(), kDocxMimeType, kDocxFileExtension),
            tasks_[selected_task]);

  // Check that the selected task has been made the default for pptx files.
  ASSERT_EQ(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                *profile()->GetPrefs(), kPptxMimeType, kPptxFileExtension),
            tasks_[selected_task]);

  // Check that the selected task has not been made the default for xlsx files
  // because there was not an xlsx file selected by the user, even though the
  // task supports xlsx files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kXlsxMimeType, kXlsxFileExtension));

  histogram_.ExpectUniqueSample(
      ash::cloud_upload::kGoogleDriveTaskResultMetricName,
      ash::cloud_upload::OfficeTaskResult::kLocalFileTask, 1);

  // cloud_open_metrics should have been destroyed by the end of the test.
  ASSERT_TRUE(cloud_open_metrics_weak_ptr.WasInvalidated());
}

IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, DefaultSetForDocsOnly) {
  // Check that the Setup flow has never run and so the File
  // Handler dialog will be launched when CloudOpenTask::Execute() is
  // called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog and get the web contents of the dialog to be
  // able to query `FileHandlerPageElement`.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, CloudProvider::kGoogleDrive,
          std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                             /*file_count=*/1),
          "file-handler-page");

  // Wait for local tasks to be filled in, which indicates the dialog is ready.
  ASSERT_TRUE(base::test::RunUntil([&] {
    // It is possible that the `FileHandlerPageElement` element still hasn't
    // been initiated yet. It is completed when the `localTasks` member is
    // non-empty.
    content::EvalJsResult eval_result =
        content::EvalJs(web_contents,
                        "document.querySelector('file-handler-page')"
                        ".localTasks.map(task => task.appId)");
    if (!eval_result.error.empty()) {
      return false;
    }
    return !eval_result.ExtractList().TakeList().empty();
  }));

  // Check that there is not a default task for doc/x files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocxMimeType, kDocxFileExtension));

  // Check that there is not a default task for ppt/x files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kPptMimeType, kPptFileExtension));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kPptxMimeType, kPptxFileExtension));

  // Click the Docs task.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      "document.querySelector('file-handler-page').$('#drive').click()"));

  content::TestNavigationObserver navigation_observer_move_page(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_move_page.StartWatchingNewWebContents();

  // Click the open button.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('file-handler-page')"
                              ".$('.action-button').click()"));

  // Wait for the move confirmation dialog to open, so we know that the File
  // Handler dialog closed.
  navigation_observer_move_page.Wait();
  ASSERT_TRUE(navigation_observer_move_page.last_navigation_succeeded());

  // Check that the Setup flow has been marked complete.
  ASSERT_TRUE(file_manager::file_tasks::HasExplicitDefaultFileHandler(profile(),
                                                                      ".docx"));

  // Check that the Docs/Slides task has been made the default for doc/x and
  // ppt/x files, but the Sheets task has not been made default for xlsx files,
  // because there was not an xlsx file selected by the user.
  ASSERT_THAT(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                  *profile()->GetPrefs(), kDocMimeType, kDocFileExtension),
              IsWebDriveOfficeTask());
  ASSERT_THAT(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                  *profile()->GetPrefs(), kDocxMimeType, kDocxFileExtension),
              IsWebDriveOfficeTask());
  ASSERT_THAT(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                  *profile()->GetPrefs(), kPptMimeType, kPptFileExtension),
              IsWebDriveOfficeTask());
  ASSERT_THAT(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                  *profile()->GetPrefs(), kPptxMimeType, kPptxFileExtension),
              IsWebDriveOfficeTask());

  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kXlsxMimeType, kXlsxFileExtension));
}

// Helper to launch Files app and return its NativeWindow.
gfx::NativeWindow LaunchFilesAppAndWait(Profile* profile) {
  GURL files_swa_url = file_manager::util::GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_NONE, /*title=*/std::u16string(),
      /*current_directory_url=*/{},
      /*selection_url=*/GURL(),
      /*target_name=*/{}, /*file_types=*/{},
      /*file_type_index=*/0,
      /*search_query=*/{},
      /*show_android_picker_apps=*/false,
      /*volume_filter=*/{});
  ash::SystemAppLaunchParams params;
  params.url = files_swa_url;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                               params);
  Browser* files_app = ui_test_utils::WaitForBrowserToOpen();
  return files_app->window()->GetNativeWindow();
}

class CloudUploadDialogNoTasksBrowserTest
    : public FileHandlerDialogBrowserTest {
 public:
  CloudUploadDialogNoTasksBrowserTest()
      : FileHandlerDialogBrowserTest(/*num_tasks=*/0) {}
};

class CloudUploadDialogHandlerDisabledBrowserTest
    : public CloudUploadDialogNoTasksBrowserTest,
      public testing::WithParamInterface<bool> {};

// Suite with `true` means that `prefs::GoogleWorkspaceCloudUpload` is `allowed`
// and `prefs::MicrosoftOfficeCloudUpload` is `disallowed`. Suite with `false`
// means that `prefs::GoogleWorkspaceCloudUpload` is `disallowed` and
// `prefs::MicrosoftOfficeCloudUpload` is `allowed`.
// Tests that when only one handler is available (Google or Microsoft) in
// absence of local tasks, the user is brought directly to the move confirmation
// page instead of file handling dialog.
IN_PROC_BROWSER_TEST_P(CloudUploadDialogHandlerDisabledBrowserTest,
                       FileHandlingDialogSkipped) {
  const bool google_workspace_test = GetParam();
  auto* prefs = profile()->GetPrefs();
  if (google_workspace_test) {
    // Disable Microsoft365.
    prefs->SetString(prefs::kGoogleWorkspaceCloudUpload,
                     chromeos::cloud_upload::kCloudUploadPolicyAllowed);
    prefs->SetString(prefs::kMicrosoftOfficeCloudUpload,
                     chromeos::cloud_upload::kCloudUploadPolicyDisallowed);
  } else {
    // Disable Google Workspace.
    prefs->SetString(prefs::kGoogleWorkspaceCloudUpload,
                     chromeos::cloud_upload::kCloudUploadPolicyDisallowed);
    prefs->SetString(prefs::kMicrosoftOfficeCloudUpload,
                     chromeos::cloud_upload::kCloudUploadPolicyAllowed);

    // Perform the necessary OneDrive & Microsoft365 setup.
    file_manager::test::MountFakeProvidedFileSystemOneDrive(profile());
    file_manager::test::AddFakeWebApp(
        web_app::kMicrosoft365AppId, kDocMimeType, kDocFileExtension, "", true,
        apps::AppServiceProxyFactory::GetForProfile(profile()));
  }

  const auto& doc_file = files_[0];
  ASSERT_EQ(doc_file.path().Extension(), ".docx");

  const CloudProvider cloud_provider = google_workspace_test
                                           ? CloudProvider::kGoogleDrive
                                           : CloudProvider::kOneDrive;
  // Launch move confirmation dialog and get the web contents of the dialog to
  // be able to query `MoveConfirmationPageElement`.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), {doc_file}, cloud_provider,
          std::make_unique<CloudOpenMetrics>(cloud_provider, /*file_count=*/1),
          "move-confirmation-page");

  constexpr char kGetProviderNameScript[] = R"(
    (async () => {
      const page = document.querySelector('move-confirmation-page');
      return page.getProviderName(page.cloudProvider);
    })();
  )";

  // Validate that the confirmation page is displayed for the correct drive.
  ASSERT_EQ(content::EvalJs(web_contents, kGetProviderNameScript),
            l10n_util::GetStringUTF8(
                google_workspace_test ? IDS_OFFICE_CLOUD_PROVIDER_GOOGLE_DRIVE
                                      : IDS_OFFICE_CLOUD_PROVIDER_ONEDRIVE));
}

INSTANTIATE_TEST_SUITE_P(/**/,
                         CloudUploadDialogHandlerDisabledBrowserTest,
                         testing::Bool());

// Tests that when only Microsoft365 is available in absence of local tasks and
// the fixup flow is required, the user is brought to the M365 setup page.
IN_PROC_BROWSER_TEST_F(
    CloudUploadDialogNoTasksBrowserTest,
    OneDriveSetupDialogShownWhenFixupFlowIsNecessaryForMicrosoft365) {
  auto* prefs = profile()->GetPrefs();
  prefs->SetString(prefs::kGoogleWorkspaceCloudUpload,
                   chromeos::cloud_upload::kCloudUploadPolicyDisallowed);
  prefs->SetString(prefs::kMicrosoftOfficeCloudUpload,
                   chromeos::cloud_upload::kCloudUploadPolicyAllowed);

  const auto& doc_file = files_[0];
  ASSERT_EQ(doc_file.path().Extension(), ".docx");

  // ODFS and O365 App are not installed, so fixup will be required.

  // Launch setup dialog. This returns once the CloudUploadElement (setup page)
  // exists.
  LaunchCloudUploadDialogAndGetWebContentsForDialog(
      profile(), {doc_file}, CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1),
      "cloud-upload");
}

// Runs each test in four configurations (Google And Microsoft prefs
// respectively):
//   * `automated` and `allowed`
//   * `automated` and `disallowed`
//   * `allowed` and `automated`
//   * `disallowed` and `automated`
class FileHandlerDialogBrowserTestWithAutomatedFlow
    : public FileHandlerDialogBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::string_view, std::string_view>> {
 protected:
  // Tests that there are no explicit file handlers set for office extensions &
  // mime types.
  bool ExplicitFileHandlersForOfficeExtensionsAndMimeTypesNotSet() {
    return ExplicitFileHandlersForExtensionsAndMimeTypesNotSet(
               fm_tasks::WordGroupExtensions(),
               fm_tasks::WordGroupMimeTypes()) &&
           ExplicitFileHandlersForExtensionsAndMimeTypesNotSet(
               fm_tasks::ExcelGroupExtensions(),
               fm_tasks::ExcelGroupMimeTypes()) &&
           ExplicitFileHandlersForExtensionsAndMimeTypesNotSet(
               fm_tasks::PowerPointGroupExtensions(),
               fm_tasks::PowerPointGroupMimeTypes());
  }

  // Tests that there are no explicit file handlers set for the given extensions
  // and mime types.
  bool ExplicitFileHandlersForExtensionsAndMimeTypesNotSet(
      const std::set<std::string>& extensions,
      const std::set<std::string>& mime_types) {
    const auto& prefs = *profile()->GetPrefs();
    for (const auto& extension : extensions) {
      if (fm_tasks::GetDefaultTaskFromPrefs(prefs,
                                            /*mime_type=*/{}, extension)) {
        return false;
      }
    }
    for (const auto& mime_type : mime_types) {
      fm_tasks::TaskDescriptor default_task;
      if (fm_tasks::GetDefaultTaskFromPrefs(prefs, mime_type,
                                            /*suffix=*/{})) {
        return false;
      }
    }
    return true;
  }

  // Tests that explicit file handlers for office extensions & mime types are
  // set to the correct handler (Google Workspace or Microsoft Office depending
  // on the test configuration).
  bool ExplicitFileHandlersForOfficeExtensionsAndMimeTypesSet() {
    const bool google_workspace_test =
        chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(
            profile());
    return ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
               fm_tasks::WordGroupExtensions(), fm_tasks::WordGroupMimeTypes(),
               google_workspace_test ? fm_tasks::kActionIdWebDriveOfficeWord
                                     : fm_tasks::kActionIdOpenInOffice) &&
           ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
               fm_tasks::ExcelGroupExtensions(),
               fm_tasks::ExcelGroupMimeTypes(),
               google_workspace_test ? fm_tasks::kActionIdWebDriveOfficeExcel
                                     : fm_tasks::kActionIdOpenInOffice) &&
           ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
               fm_tasks::PowerPointGroupExtensions(),
               fm_tasks::PowerPointGroupMimeTypes(),
               google_workspace_test
                   ? fm_tasks::kActionIdWebDriveOfficePowerPoint
                   : fm_tasks::kActionIdOpenInOffice);
  }

  // Tests that there are no explicit file handlers set for the given extensions
  // and mime types are set to a task with a given |action_id|.
  bool ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      const std::set<std::string>& extensions,
      const std::set<std::string>& mime_types,
      std::string_view action_id) {
    return ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
        extensions, mime_types,
        /*task=*/
        fm_tasks::TaskDescriptor(file_manager::kFileManagerSwaAppId,
                                 fm_tasks::TASK_TYPE_WEB_APP,
                                 fm_tasks::ToSwaActionId(action_id)));
  }

  // Tests that there are no explicit file handlers set for the given extensions
  // and mime types are set to a particular |task|.
  bool ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      const std::set<std::string>& extensions,
      const std::set<std::string>& mime_types,
      const fm_tasks::TaskDescriptor& task) {
    const auto& prefs = *profile()->GetPrefs();
    for (const auto& extension : extensions) {
      if (fm_tasks::GetDefaultTaskFromPrefs(prefs,
                                            /*mime_type=*/{},
                                            extension) != task) {
        return false;
      }
    }
    for (const auto& mime_type : mime_types) {
      if (fm_tasks::GetDefaultTaskFromPrefs(prefs, mime_type, /*suffix=*/{}) !=
          task) {
        return false;
      }
    }
    return true;
  }

  // Assigns Google & Microsoft prefs to param values for this test.
  void AssignClippyPrefs() {
    auto [google_workspace_cloud_upload, microsoft_office_cloud_upload] =
        GetParam();
    auto* prefs = profile()->GetPrefs();
    prefs->SetString(prefs::kGoogleWorkspaceCloudUpload,
                     google_workspace_cloud_upload);
    prefs->SetString(prefs::kMicrosoftOneDriveMount,
                     microsoft_office_cloud_upload);
    prefs->SetString(prefs::kMicrosoftOfficeCloudUpload,
                     microsoft_office_cloud_upload);
  }
};

// Tests that when one handler is automated, the user is brought directly to the
// move confirmation page instead of file handling dialog.
IN_PROC_BROWSER_TEST_P(FileHandlerDialogBrowserTestWithAutomatedFlow,
                       AutomatedClippyFlow) {
  EXPECT_TRUE(ExplicitFileHandlersForOfficeExtensionsAndMimeTypesNotSet());

  AssignClippyPrefs();

  if (chromeos::cloud_upload::IsMicrosoftOfficeCloudUploadAutomated(
          profile())) {
    // Perform the necessary OneDrive & Microsoft365 setup.
    file_manager::test::MountFakeProvidedFileSystemOneDrive(profile());
    file_manager::test::AddFakeWebApp(
        web_app::kMicrosoft365AppId, kDocMimeType, kDocFileExtension, "", true,
        apps::AppServiceProxyFactory::GetForProfile(profile()));
  }

  EXPECT_TRUE(ExplicitFileHandlersForOfficeExtensionsAndMimeTypesSet());

  const CloudProvider cloud_provider =
      chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile())
          ? CloudProvider::kGoogleDrive
          : CloudProvider::kOneDrive;
  // Launch move confirmation dialog and get the web contents of the dialog to
  // be able to query `MoveConfirmationPageElement`.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, cloud_provider,
          std::make_unique<CloudOpenMetrics>(cloud_provider, /*file_count=*/1),
          "move-confirmation-page");

  constexpr char kGetProviderNameScript[] = R"(
    (async () => {
      const page = document.querySelector('move-confirmation-page');
      return page.getProviderName(page.cloudProvider);
    })();
  )";

  // Validate that the confirmation page is displayed for the correct drive.
  ASSERT_EQ(content::EvalJs(web_contents, kGetProviderNameScript),
            l10n_util::GetStringUTF8(
                chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(
                    profile())
                    ? IDS_OFFICE_CLOUD_PROVIDER_GOOGLE_DRIVE
                    : IDS_OFFICE_CLOUD_PROVIDER_ONEDRIVE));
}

// Tests that toggling the pref to `automated` and then back to `disallowed`
// first assigns and then reset office file handlers.
IN_PROC_BROWSER_TEST_P(FileHandlerDialogBrowserTestWithAutomatedFlow,
                       InvertFileHandlers) {
  // Initially no handlers should be set.
  EXPECT_TRUE(ExplicitFileHandlersForOfficeExtensionsAndMimeTypesNotSet());

  AssignClippyPrefs();

  // Now all office handlers are set to either Google or Microsoft (depending on
  // the test param).
  EXPECT_TRUE(ExplicitFileHandlersForOfficeExtensionsAndMimeTypesSet());

  // Now toggle the automated policy to disallowed.
  profile()->GetPrefs()->SetString(
      chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile())
          ? prefs::kGoogleWorkspaceCloudUpload
          : prefs::kMicrosoftOfficeCloudUpload,
      kCloudUploadPolicyDisallowed);

  // All handlers are reset.
  EXPECT_TRUE(ExplicitFileHandlersForOfficeExtensionsAndMimeTypesNotSet());
}

// Tests that toggling the pref to `automated` and then back to `disallowed`
// respects existing user prefs if they're set to other handlers for selected
// office extensions or mime types.
IN_PROC_BROWSER_TEST_P(FileHandlerDialogBrowserTestWithAutomatedFlow,
                       PreexistingFileHandlersAreNotOverwritten) {
  std::set<std::string> test_extensions({".docx", ".pptx", ".xslx"});
  std::set<std::string> test_mime_types({"application/msword",
                                         "application/vnd.ms-excel",
                                         "application/vnd.ms-excel"});

  // Create sets of office extensions excluding the ones reserved for testing.
  std::set<std::string> word_extensions(fm_tasks::WordGroupExtensions());
  std::set<std::string> excel_extensions(fm_tasks::ExcelGroupExtensions());
  std::set<std::string> power_point_extensions(
      fm_tasks::PowerPointGroupExtensions());
  for (const auto& extension : test_extensions) {
    word_extensions.erase(extension);
    excel_extensions.erase(extension);
    power_point_extensions.erase(extension);
  }

  // Create sets of office mime types excluding the ones reserved for testing.
  std::set<std::string> word_mime_types(fm_tasks::WordGroupMimeTypes());
  std::set<std::string> excel_mime_types(fm_tasks::ExcelGroupMimeTypes());
  std::set<std::string> power_point_mime_types(
      fm_tasks::PowerPointGroupMimeTypes());
  for (const auto& mime_type : test_mime_types) {
    word_mime_types.erase(mime_type);
    excel_mime_types.erase(mime_type);
    power_point_mime_types.erase(mime_type);
  }

  // Initially no handlers should be set.
  EXPECT_TRUE(ExplicitFileHandlersForOfficeExtensionsAndMimeTypesNotSet());

  constexpr char kAppId[] = "app_id";
  constexpr char kActionId[] = "action_id";
  const fm_tasks::TaskDescriptor descriptor(
      kAppId, fm_tasks::TASK_TYPE_FILE_HANDLER, kActionId);
  // Imitate a user setting default preferences for selected extensions & mime
  // types.
  fm_tasks::UpdateDefaultTask(profile(), descriptor, test_extensions,
                              test_mime_types);

  // Check that the handlers have been propagated.
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      test_extensions, test_mime_types, descriptor));

  AssignClippyPrefs();

  // Check that user-selected handlers remain unchanged on `automated`.
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      test_extensions, test_mime_types, descriptor));

  // Check that other office extensions & mime types are set to Google or
  // Microsoft.
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      word_extensions, word_mime_types,
      chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile())
          ? fm_tasks::kActionIdWebDriveOfficeWord
          : fm_tasks::kActionIdOpenInOffice));
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      excel_extensions, excel_mime_types,
      chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile())
          ? fm_tasks::kActionIdWebDriveOfficeExcel
          : fm_tasks::kActionIdOpenInOffice));
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      power_point_extensions, power_point_mime_types,
      chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile())
          ? fm_tasks::kActionIdWebDriveOfficePowerPoint
          : fm_tasks::kActionIdOpenInOffice));

  // Now toggle the automated policy to disallowed.
  profile()->GetPrefs()->SetString(
      chromeos::cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile())
          ? prefs::kGoogleWorkspaceCloudUpload
          : prefs::kMicrosoftOfficeCloudUpload,
      kCloudUploadPolicyDisallowed);

  // Check that user-selected handlers remain unchanged on `disallowed`.
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesSetTo(
      test_extensions, test_mime_types, descriptor));

  // Check that other office extensions & mime types are reset.
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesNotSet(
      word_extensions, word_mime_types));
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesNotSet(
      excel_extensions, excel_mime_types));
  EXPECT_TRUE(ExplicitFileHandlersForExtensionsAndMimeTypesNotSet(
      power_point_extensions, power_point_mime_types));
}

INSTANTIATE_TEST_SUITE_P(
    /**/,
    FileHandlerDialogBrowserTestWithAutomatedFlow,
    testing::ValuesIn(
        std::vector<std::tuple<std::string_view, std::string_view>>(
            {{kCloudUploadPolicyAutomated, kCloudUploadPolicyAllowed},
             {kCloudUploadPolicyAutomated, kCloudUploadPolicyDisallowed},
             {kCloudUploadPolicyAllowed, kCloudUploadPolicyAutomated},
             {kCloudUploadPolicyDisallowed, kCloudUploadPolicyAutomated}})));

IN_PROC_BROWSER_TEST_F(
    FileHandlerDialogBrowserTest,
    ShowConnectOneDriveDialogWithModalParent_OpensAndClosesDialog) {
  // Watch for the Connect OneDrive dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch the Connect OneDrive dialog on top of a files app.
  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait(browser()->profile());
  ASSERT_TRUE(ShowConnectOneDriveDialog(modal_parent));

  // Wait for chrome://cloud-upload to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Check that we have the right DOM element (Connect OneDrive).
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();
  WaitUntilElementExists(web_contents, "connect-onedrive");

  // Click the close button and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('connect-onedrive')"
                              ".$('.cancel-button').click()"));
  watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(
    FileHandlerDialogBrowserTest,
    ShowConnectOneDriveDialogWithoutModalParent_OpensAndClosesDialog) {
  // Watch for the Connect OneDrive dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch the Connect OneDrive dialog without a modal parent.
  ASSERT_TRUE(ShowConnectOneDriveDialog(nullptr));

  // Wait for chrome://cloud-upload to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Check that we have the right DOM element (Connect OneDrive).
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();
  WaitUntilElementExists(web_contents, "connect-onedrive");

  // Click the close button and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('connect-onedrive')"
                              ".$('.cancel-button').click()"));
  watcher.Wait();
}

// Tests that OnSetupDialogComplete() opens the specified fake file task.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       OnSetupDialogCompleteOpensFileTasks) {
  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();
  {
    file_manager::file_tasks::TaskDescriptor default_task;

    auto cloud_open_task = base::WrapRefCounted(new CloudOpenTask(
        profile(), files_, file_manager::file_tasks::TaskDescriptor(),
        CloudProvider::kGoogleDrive, std::move(cloud_open_metrics)));
    cloud_open_task->SetTasksForTest(tasks_);

    for (int selected_task = 0; selected_task < num_tasks_; selected_task++) {
      std::string user_response = base::NumberToString(selected_task);
      // Watch for the selected task to open.
      content::TestNavigationObserver navigation_observer_task(
          (GURL(urls_[selected_task])));
      navigation_observer_task.StartWatchingNewWebContents();

      // Simulate user selecting this task.
      cloud_open_task->OnSetupDialogComplete(user_response);

      // Wait for the selected task to open.
      navigation_observer_task.Wait();

      // Check that the selected task has been made the default.
      ASSERT_EQ(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                    *profile()->GetPrefs(), kPptxMimeType, kPptxFileExtension),
                tasks_[selected_task]);
    }
  }
  // cloud_open_metrics should have been destroyed by the end of the test.
  ASSERT_TRUE(cloud_open_metrics_weak_ptr.WasInvalidated());
}

// Tests that OnSetupDialogComplete() doesn't crash when the specified selected
// task doesn't exist.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       OnSetupDialogCompleteNoCrash) {
  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();
  {
    auto cloud_open_task = base::WrapRefCounted(new CloudOpenTask(
        profile(), files_, file_manager::file_tasks::TaskDescriptor(),
        CloudProvider::kGoogleDrive, std::move(cloud_open_metrics)));
    cloud_open_task->SetTasksForTest(tasks_);

    int out_of_range_task = num_tasks_;
    std::string user_response = base::NumberToString(out_of_range_task);

    // Simulate user selecting a nonexistent selected task.
    cloud_open_task->OnSetupDialogComplete(user_response);
  }
  // cloud_open_metrics should have been destroyed by the end of the test.
  ASSERT_TRUE(cloud_open_metrics_weak_ptr.WasInvalidated());
}

// Tests the Fixup flow. Ensures that it is run when the conditions are met: the
// Setup flow has completed, ODFS is not mounted or the Office PWA is not
// installed and OneDrive is selected as the cloud provider. Ensures that it
// cannot change the default task set.
class FixUpFlowBrowserTest : public InProcessBrowserTest {
 public:
  FixUpFlowBrowserTest() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
  }

  FixUpFlowBrowserTest(const FixUpFlowBrowserTest&) = delete;
  FixUpFlowBrowserTest& operator=(const FixUpFlowBrowserTest&) = delete;

  Profile* profile() { return browser()->profile(); }

  void SetUpOnMainThread() override {
    // Needed to check that Files app was launched as the dialog's modal parent.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
  }

  // Add a doc test file.
  void SetUpFiles() {
    base::FilePath file =
        file_manager::util::GetMyFilesFolderForProfile(profile()).AppendASCII(
            "foo.doc");
    files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()), file));
  }

  void AddFakeODFS() {
    file_manager::test::MountFakeProvidedFileSystemOneDrive(profile());
  }

  void AddFakeOfficePWA() {
    file_manager::test::AddFakeWebApp(
        web_app::kMicrosoft365AppId, kDocMimeType, kDocFileExtension, "", true,
        apps::AppServiceProxyFactory::GetForProfile(profile()));
  }

 protected:
  // Use a non-managed user in this browser test to ensure
  // |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
  // |IsUploadOfficeToCloudEnabled|.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpCommandLineForNonManagedUser(command_line);
  }
  std::vector<storage::FileSystemURL> files_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

using file_manager::file_tasks::kActionIdWebDriveOfficeWord;
using file_manager::file_tasks::SetWordFileHandlerToFilesSWA;

// Tests that the Fixup flow is entered when OneDrive is selected as the cloud
// provider but ODFS is not mounted and the Setup flow has already completed.
// Checks that the ODFS Sign In Page is reachable.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest, FixUpFlowWhenODFSNotMounted) {
  // Simulate prefs where the setup flow has already run.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  SetUpFiles();
  AddFakeOfficePWA();

  // ODFS is not mounted, expect that the Fixup flow will need to run.
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));

  LaunchFilesAppAndWait(browser()->profile());

  // Launch setup and get the web contents of the dialog to be able to
  // query `CloudUploadElement`.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, CloudProvider::kOneDrive,
          std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                             /*file_count=*/1),
          "cloud-upload");

  // Wait until the "cloud-upload" DOM element is properly initialised.
  WaitUntilJsReturnsTrue(
      web_contents,
      "!!document.querySelector('cloud-upload').$('welcome-page')");

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('welcome-page')"
      ".querySelector('.action-button').click()")) {
  }

  // Wait for the ODFS Sign In Page.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('sign-in-page')"
      ".querySelector('.action-button')")) {
  }
}

// Tests that the Fixup flow is entered when OneDrive is selected as the cloud
// provider but the Office PWA is not installed and the Setup flow has already
// completed. Checks that the Office PWA Install Page is reachable.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest,
                       FixUpFlowWhenOfficePWANotInstalled) {
  // Simulate prefs where the setup flow has already run.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  SetUpFiles();
  AddFakeODFS();

  // Office PWA is not installed, expect that the Fixup flow will need to run.
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));

  LaunchFilesAppAndWait(browser()->profile());

  // Launch setup and get the web contents of the dialog to be able to
  // query `CloudUploadElement`.
  content::WebContents* web_contents =
      LaunchCloudUploadDialogAndGetWebContentsForDialog(
          profile(), files_, CloudProvider::kOneDrive,
          std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                             /*file_count=*/1),
          "cloud-upload");

  // Wait until the "cloud-upload" DOM element is properly initialised.
  WaitUntilJsReturnsTrue(
      web_contents,
      "!!document.querySelector('cloud-upload').$('welcome-page')");

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('welcome-page')"
      ".querySelector('.action-button').click()")) {
  }

  // Wait for the Office PWA Install Page, this script will fail until the page
  // exists.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('office-pwa-install-page')"
      ".querySelector('.action-button')")) {
  }
}

// Tests that the existing Fixup/Setup dialog is brought to the front when
// trying to launch a second one for a different file.
IN_PROC_BROWSER_TEST_F(
    FixUpFlowBrowserTest,
    FixUpDialogBroughtToFrontWhenFailingToLaunchADifferentOne) {
  // Simulate prefs where the setup flow has already run.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  SetUpFiles();

  // ODFS is not mounted, expect that the Fixup flow will need to run.
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));

  gfx::NativeWindow modal_parent1 = LaunchFilesAppAndWait(browser()->profile());

  // Launch the setup dialog at chrome://cloud-upload.
  LaunchCloudUploadDialogAndGetWebContentsForDialog(
      profile(), files_, CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1),
      "cloud-upload");

  gfx::NativeWindow modal_parent2 = LaunchFilesAppAndWait(browser()->profile());

  auto* modal_parent_widget1 =
      views::Widget::GetWidgetForNativeWindow(modal_parent1);
  auto* modal_parent_widget2 =
      views::Widget::GetWidgetForNativeWindow(modal_parent2);

  // The second files app would have launched above the setup dialog that is
  // modal to the first files app.
  ASSERT_TRUE(modal_parent_widget2->IsStackedAbove(
      modal_parent_widget1->GetNativeView()));
  ASSERT_TRUE(modal_parent_widget2->is_top_level());

  // A second setup dialog cannot be launched at chrome://cloud-upload as there
  // is already the setup dialog.
  base::FilePath file =
      file_manager::util::GetMyFilesFolderForProfile(profile()).AppendASCII(
          "foo2.doc");
  std::vector<storage::FileSystemURL> files;
  files.push_back(FilePathToFileSystemURL(
      profile(), file_manager::util::GetFileManagerFileSystemContext(profile()),
      file));
  ASSERT_FALSE(CloudOpenTask::Execute(
      profile(), files, file_manager::file_tasks::TaskDescriptor(),
      CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1)));

  // The setup dialog would have been brought to the front.
  ASSERT_TRUE(modal_parent_widget1->IsStackedAbove(
      modal_parent_widget2->GetNativeView()));
  ASSERT_TRUE(modal_parent_widget1->is_top_level());
}

// Tests that the existing Fixup/Setup dialog is brought to the front when
// trying to launch a second one for the same file.
IN_PROC_BROWSER_TEST_F(
    FixUpFlowBrowserTest,
    FixUpDialogBroughtToFrontWhenFailingToLaunchADuplicateOne) {
  // Simulate prefs where the setup flow has already run.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  SetUpFiles();

  // ODFS is not mounted, expect that the Fixup flow will need to run.
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));

  gfx::NativeWindow modal_parent1 = LaunchFilesAppAndWait(browser()->profile());

  // Launch the setup dialog at chrome://cloud-upload.
  LaunchCloudUploadDialogAndGetWebContentsForDialog(
      profile(), files_, CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1),
      "cloud-upload");

  gfx::NativeWindow modal_parent2 = LaunchFilesAppAndWait(browser()->profile());

  auto* modal_parent_widget1 =
      views::Widget::GetWidgetForNativeWindow(modal_parent1);
  auto* modal_parent_widget2 =
      views::Widget::GetWidgetForNativeWindow(modal_parent2);

  // The second files app would have launched above the setup dialog that is
  // modal to the first files app.
  ASSERT_TRUE(modal_parent_widget2->IsStackedAbove(
      modal_parent_widget1->GetNativeView()));
  ASSERT_TRUE(modal_parent_widget2->is_top_level());

  // A duplicate setup dialog cannot be launched at chrome://cloud-upload as
  // there is already the setup dialog.
  ASSERT_FALSE(CloudOpenTask::Execute(
      profile(), files_, file_manager::file_tasks::TaskDescriptor(),
      CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1)));

  // The setup dialog would have been brought to the front.
  ASSERT_TRUE(modal_parent_widget1->IsStackedAbove(
      modal_parent_widget2->GetNativeView()));
  ASSERT_TRUE(modal_parent_widget1->is_top_level());
}

// Tests that the existing Fixup/Setup dialog is brought to the front when
// trying to launch a second one via `ShowConnectOneDriveDialog()`.
IN_PROC_BROWSER_TEST_F(
    FixUpFlowBrowserTest,
    FixUpDialogBroughtToFrontWhenShowConnectOneDriveDialogFailsToLaunch) {
  // Simulate prefs where the setup flow has already run.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  SetUpFiles();

  // ODFS is not mounted, expect that the Fixup flow will need to run.
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));

  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait(browser()->profile());

  // Launch the setup dialog at chrome://cloud-upload.
  LaunchCloudUploadDialogAndGetWebContentsForDialog(
      profile(), files_, CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1),
      "cloud-upload");

  // Launch a settings page.
  ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::SETTINGS);
  Browser* files_app = ui_test_utils::WaitForBrowserToOpen();
  gfx::NativeWindow settings = files_app->window()->GetNativeWindow();

  auto* modal_parent_widget =
      views::Widget::GetWidgetForNativeWindow(modal_parent);
  auto* settings_widget = views::Widget::GetWidgetForNativeWindow(settings);

  // The settings would have launched above the setup dialog that is modal to
  // the first files app.
  ASSERT_TRUE(
      settings_widget->IsStackedAbove(modal_parent_widget->GetNativeView()));
  ASSERT_TRUE(settings_widget->is_top_level());

  // The Connect OneDrive dialog cannot be launched at chrome://cloud-upload as
  // there is already the setup dialog.
  ASSERT_FALSE(ShowConnectOneDriveDialog(nullptr));

  // The setup dialog would have been brought to the front.
  ASSERT_TRUE(
      modal_parent_widget->IsStackedAbove(settings_widget->GetNativeView()));
  ASSERT_TRUE(modal_parent_widget->is_top_level());
}

// Tests that `ShouldFixUpOffice()` returns true when neither ODFS is mounted
// nor Office PWA is installed and OneDrive is selected as the cloud provider.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest, ShouldFixUpOfficeNoODFSNoPWA) {
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));
}

// Tests that `ShouldFixUpOffice()` returns false when neither ODFS is mounted
// nor Office PWA is installed but Drive is selected as the cloud provider.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest, ShouldFixUpOfficeDrive) {
  ASSERT_FALSE(ShouldFixUpOffice(profile(), CloudProvider::kGoogleDrive));
}

// Tests that `ShouldFixUpOffice()` returns false when both ODFS is mounted and
// Office PWA is installed and OneDrive is selected as the cloud provider.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest, ShouldFixUpOfficeODFSAndPWA) {
  AddFakeODFS();
  AddFakeOfficePWA();
  ASSERT_FALSE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));
}

// Test that entering and completing the Setup flow from the OneDrive Set Up
// point changes the default task set when the Setup has not been run before.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest,
                       OneDriveSetUpChangesDefaultTaskWhenSetUpIncomplete) {
  // Simulate Setup flow incomplete - prefs are empty to begin with.

  // Add a doc test file.
  SetUpFiles();
  AddFakeODFS();
  AddFakeOfficePWA();

  auto cloud_open_task = base::WrapRefCounted(new CloudOpenTask(
      profile(), files_, file_manager::file_tasks::TaskDescriptor(),
      CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1)));
  mojom::DialogArgsPtr args =
      cloud_open_task->CreateDialogArgs(SetupOrMoveDialogPage::kOneDriveSetup);
  // Self-deleted on close.
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), base::DoNothing(), false);

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Check that there is not a default task for doc or xlsx files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension));
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kXlsxMimeType, kXlsxFileExtension));

  // Open the Welcome Page for the OneDrive set up part of the Setup flow.
  // This will lead to the Office PWA being set as the default task.
  dialog->ShowSystemDialog();

  // Wait for chrome://cloud-upload to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Wait until the "cloud-upload" DOM element is properly initialised.
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();
  WaitUntilJsReturnsTrue(
      web_contents,
      "!!document.querySelector('cloud-upload').$('welcome-page')");

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('welcome-page')"
      ".querySelector('.action-button').click()")) {
  }

  // Click through the Upload Page.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('complete-page')"
      ".querySelector('.action-button').click()")) {
  }

  // Check that the Office PWA has been made the default for doc and xlsx
  // files.
  ASSERT_THAT(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                  *profile()->GetPrefs(), kDocMimeType, kDocFileExtension),
              IsOpenInOfficeTask());
  ASSERT_THAT(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                  *profile()->GetPrefs(), kXlsxMimeType, kXlsxFileExtension),
              IsOpenInOfficeTask());
}

// Test that entering and completing the Setup flow from the OneDrive Set Up
// point does not change the default task set when there was already a default
// handler before setup. This is to test that when the Fixup flow runs, the
// default task does not change.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest,
                       OneDriveSetUpDoesNotChangeDefaultTaskWhenSetUpComplete) {
  // Simulate prefs where the setup flow has already run.
  SetWordFileHandlerToFilesSWA(profile(), kActionIdWebDriveOfficeWord);

  // Add a doc test file.
  SetUpFiles();
  // Note: although mounting ODFS and installing the Office PWA sets up
  // conditions so that the Fixup flow does not need to be run, this test is
  // just to check that entering the Setup flow from OneDrive Setup point does
  // not set the default task when the Setup flow is already complete.
  // Otherwise, the test would get stuck trying to set up OneDrive, unable to
  // navigate through all the dialog pages.
  AddFakeODFS();
  AddFakeOfficePWA();

  auto cloud_open_task = base::WrapRefCounted(new CloudOpenTask(
      profile(), files_, file_manager::file_tasks::TaskDescriptor(),
      CloudProvider::kOneDrive,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                         /*file_count=*/1)));
  mojom::DialogArgsPtr args =
      cloud_open_task->CreateDialogArgs(SetupOrMoveDialogPage::kOneDriveSetup);
  // Self-deleted on close.
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), base::DoNothing(), false);

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Open the Welcome Page for the OneDrive set up part of the Setup flow. This
  // will not lead to the Office PWA being set as the default task because there
  // was already a default before running setup.
  dialog->ShowSystemDialog();

  // Wait for chrome://cloud-upload to open.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Wait until the "cloud-upload" DOM element is properly initialised.
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();
  WaitUntilJsReturnsTrue(
      web_contents,
      "!!document.querySelector('cloud-upload').$('welcome-page')");

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('welcome-page')"
      ".querySelector('.action-button').click()")) {
  }

  // Click through the Upload Page.
  while (!content::ExecJs(
      web_contents,
      "document.querySelector('cloud-upload').$('complete-page')"
      ".querySelector('.action-button').click()")) {
  }

  // Check that the default task for doc files is still Drive, and not OneDrive,
  // despite running fixup setup.
  ASSERT_THAT(file_manager::file_tasks::GetDefaultTaskFromPrefs(
                  *profile()->GetPrefs(), kDocMimeType, kDocFileExtension),
              testing::Optional(testing::Field(
                  &file_manager::file_tasks::TaskDescriptor::action_id,
                  testing::EndsWith(kActionIdWebDriveOfficeWord))));
}

class CloudOpenTaskBrowserTest : public InProcessBrowserTest {
 public:
  CloudOpenTaskBrowserTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    my_files_dir_ = temp_dir_.GetPath().Append("myfiles");
    read_only_dir_ = temp_dir_.GetPath().Append("readonly");
    smb_dir_ = temp_dir_.GetPath().Append("smb");
  }

  CloudOpenTaskBrowserTest(const CloudOpenTaskBrowserTest&) = delete;
  CloudOpenTaskBrowserTest& operator=(const CloudOpenTaskBrowserTest&) = delete;

  void TearDownOnMainThread() override {
    // Explictly destroy the `upload_task_` before the Profile* is destroyed.
    // Otherwise the `upload_task_` will be destroyed afterwards and the profile
    // pointer owned by the `upload_task_` will become dangling
    upload_task_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpLocalToDriveTask() {
    SetUpMyFiles();

    source_files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        my_files_dir_.AppendASCII("file.docx")));

    upload_task_ = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
        profile(), source_files_, file_manager::file_tasks::TaskDescriptor(),
        ash::cloud_upload::CloudProvider::kGoogleDrive,
        std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                           /*file_count=*/1)));
  }

  void SetUpCloudToDriveTask() {
    SetUpCloudLocation();

    source_files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        smb_dir_.AppendASCII("file.docx")));

    upload_task_ = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
        profile(), source_files_, file_manager::file_tasks::TaskDescriptor(),
        ash::cloud_upload::CloudProvider::kGoogleDrive,
        std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                           /*file_count=*/1)));
  }

  void SetUpReadOnlyToDriveTask() {
    SetUpReadOnlyLocation();

    source_files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        read_only_dir_.AppendASCII("file.docx")));

    upload_task_ = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
        profile(), source_files_, file_manager::file_tasks::TaskDescriptor(),
        ash::cloud_upload::CloudProvider::kGoogleDrive,
        std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                           /*file_count=*/1)));
  }

  void SetUpLocalToOneDriveTask() {
    SetUpMyFiles();

    source_files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        my_files_dir_.AppendASCII("file.docx")));

    upload_task_ = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
        profile(), source_files_, file_manager::file_tasks::TaskDescriptor(),
        ash::cloud_upload::CloudProvider::kOneDrive,
        std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                           /*file_count=*/1)));
  }

  void SetUpCloudToOneDriveTask() {
    SetUpCloudLocation();

    source_files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        smb_dir_.AppendASCII("file.docx")));

    upload_task_ = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
        profile(), source_files_, file_manager::file_tasks::TaskDescriptor(),
        ash::cloud_upload::CloudProvider::kOneDrive,
        std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                           /*file_count=*/1)));
  }

  void SetUpReadOnlyToOneDriveTask() {
    SetUpReadOnlyLocation();

    source_files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        read_only_dir_.AppendASCII("file.docx")));

    upload_task_ = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
        profile(), source_files_, file_manager::file_tasks::TaskDescriptor(),
        ash::cloud_upload::CloudProvider::kOneDrive,
        std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                           /*file_count=*/1)));
  }

  bool ShouldShowConfirmationDialog() {
    return upload_task_->ShouldShowConfirmationDialog();
  }

  void OnMoveConfirmationComplete(const std::string& user_response) {
    upload_task_->OnMoveConfirmationComplete(user_response);
  }

  Profile* profile() { return browser()->profile(); }

 protected:
  // Use a non-managed user in this browser test to ensure
  // |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
  // |IsUploadOfficeToCloudEnabled|.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpCommandLineForNonManagedUser(command_line);
  }

 private:
  // Creates mount point for MyFiles and registers local filesystem.
  void SetUpMyFiles() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    }
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile());
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), my_files_dir_));
    file_manager::VolumeManager::Get(profile())
        ->RegisterDownloadsDirectoryForTesting(my_files_dir_);
  }

  // Creates a new SMB filesystem, which we use in tests as an example of cloud
  // location.
  void SetUpCloudLocation() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(smb_dir_));
    }
    std::string mount_point_name = "smb";
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    EXPECT_TRUE(profile()->GetMountPoints()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), smb_dir_));
    file_manager::VolumeManager::Get(profile())->AddVolumeForTesting(
        smb_dir_, file_manager::VOLUME_TYPE_SMB, ash::DeviceType::kUnknown,
        /*read_only=*/false);
  }

  // Creates a new filesystem which represents a read-only location, files
  // cannot be moved from it.
  void SetUpReadOnlyLocation() {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateDirectory(read_only_dir_));
    }
    std::string mount_point_name = "readonly";
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        mount_point_name);
    EXPECT_TRUE(profile()->GetMountPoints()->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), read_only_dir_));
    file_manager::VolumeManager::Get(profile())->AddVolumeForTesting(
        read_only_dir_, file_manager::VOLUME_TYPE_TESTING,
        ash::DeviceType::kUnknown, /*read_only=*/true);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_dir_;
  base::FilePath read_only_dir_;
  base::FilePath smb_dir_;
  std::vector<storage::FileSystemURL> source_files_;
  scoped_refptr<CloudOpenTask> upload_task_;
  std::unique_ptr<CloudOpenMetrics> cloud_open_metrics_;
};

// Tests that when moving files from a local location to Drive, the preferences
// |kOfficeFilesAlwaysMoveToDrive| and
// |kOfficeMoveConfirmationShownForLocalToDrive| control whether the
// confirmation dialog is going to be shown or not.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       ShowConfirmationForLocalToDrive) {
  SetUpLocalToDriveTask();

  // Check that if |kOfficeMoveConfirmationShownForLocalToDrive| is false, we
  // always show the confirmation dialog, whether
  // |kOfficeFilesAlwaysMoveToDrive| is true or false.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToDrive(
      profile(), false);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), true);
  ASSERT_TRUE(ShouldShowConfirmationDialog());

  // Check that if |kOfficeMoveConfirmationShownForLocalToDrive| is true, we
  // only show the confirmation dialog depending on the value of
  // |kOfficeFilesAlwaysMoveToDrive|.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToDrive(
      profile(), true);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), true);
  ASSERT_FALSE(ShouldShowConfirmationDialog());
}

// Tests that when moving files from a cloud location to Drive, the preferences
// |kOfficeFilesAlwaysMoveToDrive| and
// |kOfficeMoveConfirmationShownForCloudToDrive| control whether the
// confirmation dialog is going to be shown or not.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       ShowConfirmationForCloudToDrive) {
  SetUpCloudToDriveTask();

  // Check that if |kOfficeMoveConfirmationShownForCloudToDrive| is false, we
  // always show the confirmation dialog, whether
  // |kOfficeFilesAlwaysMoveToDrive| is true or false.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToDrive(
      profile(), false);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), true);
  ASSERT_TRUE(ShouldShowConfirmationDialog());

  // Check that if |kOfficeMoveConfirmationShownForLocalToDrive| is true, we
  // only show the confirmation dialog depending on the value of
  // |kOfficeFilesAlwaysMoveToDrive|.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToDrive(
      profile(), true);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), true);
  ASSERT_FALSE(ShouldShowConfirmationDialog());
}

// Tests that when moving files from a read-only location to Drive, the
// preferences |kOfficeFilesAlwaysMoveToDrive|,
// |kOfficeMoveConfirmationShownForLocalToDrive| and
// |kOfficeMoveConfirmationShownForCloudToDrive| control whether the
// confirmation dialog is going to be shown or not.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       ShowConfirmationForReadOnlyToDrive) {
  SetUpReadOnlyToDriveTask();

  // Check that if |kOfficeMoveConfirmationShownForLocalToDrive| and
  // |kOfficeMoveConfirmationShownForCloudToDrive| are both false, we always
  // show the confirmation dialog, whether |kOfficeFilesAlwaysMoveToDrive| is
  // true or false.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToDrive(
      profile(), false);
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToDrive(
      profile(), false);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), true);
  ASSERT_TRUE(ShouldShowConfirmationDialog());

  // Check that if at least one of |kOfficeMoveConfirmationShownForLocalToDrive|
  // and |kOfficeMoveConfirmationShownForCloudToDrive| is true, we only show the
  // confirmation dialog depending on the value of
  // |kOfficeFilesAlwaysMoveToDrive|.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToDrive(
      profile(), true);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToDrive(profile(), true);
  ASSERT_FALSE(ShouldShowConfirmationDialog());
}

// Tests that when moving files from a local location to OneDrive, the
// preferences |kOfficeFilesAlwaysMoveToOneDrive| and
// |kOfficeMoveConfirmationShownForLocalToOneDrive| control whether the
// confirmation dialog is going to be shown or not.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       ShowConfirmationForLocalToOneDrive) {
  SetUpLocalToOneDriveTask();

  // Check that if |kOfficeMoveConfirmationShownForLocalToOneDrive| is false, we
  // always show the confirmation dialog, whether
  // |kOfficeFilesAlwaysMoveToOneDrive| is true or false.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToOneDrive(
      profile(), false);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(),
                                                               false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(), true);
  ASSERT_TRUE(ShouldShowConfirmationDialog());

  // Check that if |kOfficeMoveConfirmationShownForLocalToOneDrive| is true, we
  // only show the confirmation dialog depending on the value of
  // |kOfficeFilesAlwaysMoveToOneDrive|.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToOneDrive(
      profile(), true);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(),
                                                               false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(), true);
  ASSERT_FALSE(ShouldShowConfirmationDialog());
}

// Tests that when moving files from a cloud location to OneDrive, the
// preferences |kOfficeFilesAlwaysMoveToOneDrive| and
// |kOfficeMoveConfirmationShownForCloudToOneDrive| control whether the
// confirmation dialog is going to be shown or not.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       ShowConfirmationForCloudToOneDrive) {
  SetUpCloudToOneDriveTask();

  // Check that if |kOfficeMoveConfirmationShownForCloudToOneDrive| is false, we
  // always show the confirmation dialog, whether
  // |kOfficeFilesAlwaysMoveToOneDrive| is true or false.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToOneDrive(
      profile(), false);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(),
                                                               false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(), true);
  ASSERT_TRUE(ShouldShowConfirmationDialog());

  // Check that if |kOfficeMoveConfirmationShownForLocalToOneDrive| is true, we
  // only show the confirmation dialog depending on the value of
  // |kOfficeFilesAlwaysMoveToOneDrive|.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToOneDrive(
      profile(), true);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(),
                                                               false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(), true);
  ASSERT_FALSE(ShouldShowConfirmationDialog());
}

// Tests that when moving files from a read-only location to OneDrive, the
// preferences |kOfficeFilesAlwaysMoveToOneDrive|,
// |kOfficeMoveConfirmationShownForLocalToOneDrive| and
// |kOfficeMoveConfirmationShownForCloudToOneDrive| control whether the
// confirmation dialog is going to be shown or not.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       ShowConfirmationForReadOnlyToOneDrive) {
  SetUpReadOnlyToOneDriveTask();

  // Check that if |kOfficeMoveConfirmationShownForLocalToOneDrive| and
  // |kOfficeMoveConfirmationShownForCloudToOneDrive| are both false, we always
  // show the confirmation dialog, whether |kOfficeFilesAlwaysMoveToOneDrive| is
  // true or false.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForLocalToOneDrive(
      profile(), false);
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToOneDrive(
      profile(), false);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(),
                                                               false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(), true);
  ASSERT_TRUE(ShouldShowConfirmationDialog());

  // Check that if at least one of
  // |kOfficeMoveConfirmationShownForLocalToOneDrive| and
  // |kOfficeMoveConfirmationShownForCloudToOneDrive| is true, we only show the
  // confirmation dialog depending on the value of
  // |kOfficeFilesAlwaysMoveToOneDrive|.
  file_manager::file_tasks::SetOfficeMoveConfirmationShownForCloudToOneDrive(
      profile(), true);
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(),
                                                               false);
  ASSERT_TRUE(ShouldShowConfirmationDialog());
  file_manager::file_tasks::SetAlwaysMoveOfficeFilesToOneDrive(profile(), true);
  ASSERT_FALSE(ShouldShowConfirmationDialog());
}

// Tests that the preferences |kOfficeMoveConfirmationShownForDrive| and
// |kOfficeMoveConfirmationShownForLocalToDrive| is are both set to true once
// the user has confirmed the upload of a file to Drive.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       SetPrefsAfterConfirmationShownForLocalToDrive) {
  SetUpLocalToDriveTask();
  ASSERT_FALSE(file_manager::file_tasks::GetOfficeMoveConfirmationShownForDrive(
      profile()));
  ASSERT_FALSE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForLocalToDrive(
          profile()));

  OnMoveConfirmationComplete(kUserActionUploadToGoogleDrive);

  ASSERT_TRUE(file_manager::file_tasks::GetOfficeMoveConfirmationShownForDrive(
      profile()));
  ASSERT_TRUE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForLocalToDrive(
          profile()));
}

// Tests that the preferences |kOfficeMoveConfirmationShownForDrive| and
// |kOfficeMoveConfirmationShownForCloudToDrive| is are both set to true once
// the user has confirmed the upload of a file to Drive.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       SetPrefsAfterConfirmationShownForCloudToDrive) {
  SetUpCloudToDriveTask();
  ASSERT_FALSE(file_manager::file_tasks::GetOfficeMoveConfirmationShownForDrive(
      profile()));
  ASSERT_FALSE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForCloudToDrive(
          profile()));

  OnMoveConfirmationComplete(kUserActionUploadToGoogleDrive);

  ASSERT_TRUE(file_manager::file_tasks::GetOfficeMoveConfirmationShownForDrive(
      profile()));
  ASSERT_TRUE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForCloudToDrive(
          profile()));
}

// Tests that the preferences |kOfficeMoveConfirmationShownForOneDrive| and
// |kOfficeMoveConfirmationShownForLocalToOneDrive| is are both set to true once
// the user has confirmed the upload of a file to OneDrive.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       SetPrefsAfterConfirmationShownForLocalToOneDrive) {
  SetUpLocalToOneDriveTask();
  ASSERT_FALSE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForOneDrive(
          profile()));
  ASSERT_FALSE(file_manager::file_tasks::
                   GetOfficeMoveConfirmationShownForLocalToOneDrive(profile()));

  OnMoveConfirmationComplete(kUserActionUploadToOneDrive);

  ASSERT_TRUE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForOneDrive(
          profile()));
  ASSERT_TRUE(file_manager::file_tasks::
                  GetOfficeMoveConfirmationShownForLocalToOneDrive(profile()));
}

// Tests that the preferences |kOfficeMoveConfirmationShownForOneDrive| and
// |kOfficeMoveConfirmationShownForCloudToOneDrive| is are both set to true once
// the user has confirmed the upload of a file to OneDrive.
IN_PROC_BROWSER_TEST_F(CloudOpenTaskBrowserTest,
                       SetPrefsAfterConfirmationShownForCloudToOneDrive) {
  SetUpCloudToOneDriveTask();
  ASSERT_FALSE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForOneDrive(
          profile()));
  ASSERT_FALSE(file_manager::file_tasks::
                   GetOfficeMoveConfirmationShownForCloudToOneDrive(profile()));

  OnMoveConfirmationComplete(kUserActionUploadToOneDrive);

  ASSERT_TRUE(
      file_manager::file_tasks::GetOfficeMoveConfirmationShownForOneDrive(
          profile()));
  ASSERT_TRUE(file_manager::file_tasks::
                  GetOfficeMoveConfirmationShownForCloudToOneDrive(profile()));
}

}  // namespace ash::cloud_upload

void NonManagedUserWebUIBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ash::cloud_upload::SetUpCommandLineForNonManagedUser(command_line);
}
