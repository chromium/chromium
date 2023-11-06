// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <algorithm>
#include <cstddef>
#include <string>

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog_browsertest.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
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
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/gfx/native_widget_types.h"

namespace ash::cloud_upload {

namespace {

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
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = GURL(start_url);
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

// Set email (using a domain from |kNonManagedDomainPatterns|) to login a
// non-managed user. Intended to be used in the override of |SetUpCommandLine|
// from |InProcessBrowserTest| to ensure
// |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
// |IsUploadOfficeToCloudEnabled| in browser tests.
void SetUpCommandLineForNonManagedUser(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kLoginUser, "testuser@gmail.com");
  command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
}

// A matcher to verify that absl::optional<TaskDescriptor> corresponds to a Web
// Drive Office Task.
auto IsWebDriveOfficeTask() {
  return testing::Optional(testing::ResultOf(
      &file_manager::file_tasks::IsWebDriveOfficeTask, testing::Eq(true)));
}

// A matcher to verify that absl::optional<TaskDescriptor> corresponds to an
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
    feature_list_.InitAndEnableFeature(
        chromeos::features::kUploadOfficeToCloud);
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

    file_manager::test::FolderInMyFiles folder(profile());

    base::FilePath test_data_path;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
    std::string file_names[] = {"text.docx", "presentation.pptx"};

    for (const auto& file_name : file_names) {
      base::FilePath file_path =
          test_data_path.AppendASCII("chromeos/file_manager/" + file_name);
      {
        base::ScopedAllowBlockingForTesting allow_blocking;
        EXPECT_TRUE(base::PathExists(file_path));
      }
      // Copy the file into My Files.
      folder.Add({file_path});
    }

    for (const auto& path_in_my_files : folder.files()) {
      GURL url;
      CHECK(file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile(), path_in_my_files, file_manager::util::GetFileManagerURL(),
          &url));
      auto* file_system_context =
          file_manager::util::GetFileManagerFileSystemContext(profile());
      files_.push_back(file_system_context->CrackURLInFirstPartyContext(url));
    }
  }

 protected:
  // Use a non-managed user in this browser test to ensure
  // |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
  // |IsUploadOfficeToCloudEnabled|.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpCommandLineForNonManagedUser(command_line);
  }

  const int num_tasks_ = 3;
  std::vector<std::string> urls_;
  std::vector<file_manager::file_tasks::TaskDescriptor> tasks_;
  std::vector<storage::FileSystemURL> files_;
  base::HistogramTester histogram_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a new Files app window is created if no modal parent window is
// passed in.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, NewModalParentCreated) {
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());

  Browser* browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_EQ(nullptr, browser);

  // Watch for File Handler dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch File Handler dialog.
  ASSERT_TRUE(CloudOpenTask::Execute(
      profile(), files_, CloudProvider::kGoogleDrive, nullptr,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                         /*file_count=*/1)));

  // Wait for File Handler dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  browser = FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_NE(nullptr, browser);
}

// Tests that a new Files app window is created even if there is a Files app
// window open, but it's not the window that was passed in to CloudOpenTask.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       NewModalParentCreatedWithExisting) {
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());

  Browser* browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_EQ(nullptr, browser);

  // Open a files app window.
  base::RunLoop run_loop;
  file_manager::util::ShowItemInFolder(
      profile(), files_.at(0).path(),
      base::BindLambdaForTesting(
          [&run_loop](platform_util::OpenOperationResult result) {
            EXPECT_EQ(platform_util::OpenOperationResult::OPEN_SUCCEEDED,
                      result);
            run_loop.Quit();
          }));
  run_loop.Run();
  Browser* first_files_app = ui_test_utils::WaitForBrowserToOpen();

  browser = FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_NE(nullptr, browser);
  ASSERT_EQ(first_files_app, browser);

  // Watch for File Handler dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch File Handler dialog.
  ASSERT_TRUE(CloudOpenTask::Execute(
      profile(), files_, CloudProvider::kGoogleDrive, nullptr,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                         /*file_count=*/1)));
  // Check that a new browser opened.
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_NE(new_browser, first_files_app);
  ASSERT_TRUE(
      IsBrowserForSystemWebApp(new_browser, SystemWebAppType::FILE_MANAGER));

  // Wait for File Handler dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());
}

// Tests that a new Files app window is not created when there is a Files app
// window already open, and it's passed in as the modal parent.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, ModalParentProvided) {
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());

  Browser* browser =
      FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_EQ(nullptr, browser);

  // Open a files app window.
  ui_test_utils::BrowserChangeObserver browser_added_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  base::RunLoop run_loop;
  file_manager::util::ShowItemInFolder(
      profile(), files_.at(0).path(),
      base::BindLambdaForTesting(
          [&run_loop](platform_util::OpenOperationResult result) {
            EXPECT_EQ(platform_util::OpenOperationResult::OPEN_SUCCEEDED,
                      result);
            run_loop.Quit();
          }));
  run_loop.Run();
  browser_added_observer.Wait();

  browser = FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER);
  ASSERT_NE(nullptr, browser);

  // Watch for File Handler dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch File Handler dialog.
  ASSERT_TRUE(CloudOpenTask::Execute(
      profile(), files_, CloudProvider::kGoogleDrive,
      browser->window()->GetNativeWindow(),
      std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                         /*file_count=*/1)));

  // Wait for File Handler dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Check that the existing Files app window was used.
  ASSERT_EQ(browser,
            FindSystemWebAppBrowser(profile(), SystemWebAppType::FILE_MANAGER));
}

// Test which launches a `CloudUploadDialog` which in turn creates a
// `FileHandlerPageElement`. Tests that the cancel button works and the correct
// TaskResult is logged.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, CancelFileHandlerDialog) {
  // Watch for File Handler dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();

  // Check that the Setup flow has never run and so the File Handler dialog will
  // be launched when CloudOpenTask::Execute() is called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog.
  ASSERT_TRUE(CloudOpenTask::Execute(profile(), files_,
                                     CloudProvider::kGoogleDrive, nullptr,
                                     std::move(cloud_open_metrics)));

  // Wait for File Handler dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Get the web contents of the dialog to be able to query
  // `FileHandlerPageElement`.
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Click the close button and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('file-handler-page')"
                              ".$('.cancel-button').click()"));
  watcher.Wait();

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

  // Watch for File Handler dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();

  // Check that the Setup flow has never run and so the File Handler dialog will
  // be launched when CloudOpenTask::Execute() is called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog.
  ASSERT_TRUE(CloudOpenTask::Execute(profile(), files_,
                                     CloudProvider::kGoogleDrive, nullptr,
                                     std::move(cloud_open_metrics)));

  // Wait for File Handler dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Get the web contents of the dialog to be able to query
  // `FileHandlerPageElement`.
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Get the `tasks` member from the `FileHandlerPageElement` which are all of
  // the observed local file tasks.
  bool dialog_init_complete = false;
  base::Value::List observed_app_ids;
  while (!dialog_init_complete) {
    // It is possible that the `FileHandlerPageElement` element still hasn't
    // been initiated yet. It is completed when the `localTasks` member is
    // non-empty.
    content::EvalJsResult eval_result =
        content::EvalJs(web_contents,
                        "document.querySelector('file-handler-page')"
                        ".localTasks.map(task => task.appId)");
    if (!eval_result.error.empty()) {
      continue;
    }
    observed_app_ids = eval_result.ExtractList().TakeList();
    dialog_init_complete = !observed_app_ids.empty();
  }

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
  // Watch for File Handler dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Check that the Setup flow has never run and so the File
  // Handler dialog will be launched when CloudOpenTask::Execute() is
  // called.
  ASSERT_FALSE(file_manager::file_tasks::HasExplicitDefaultFileHandler(
      profile(), ".docx"));

  // Launch File Handler dialog.
  ASSERT_TRUE(CloudOpenTask::Execute(
      profile(), files_, CloudProvider::kGoogleDrive, nullptr,
      std::make_unique<CloudOpenMetrics>(CloudProvider::kGoogleDrive,
                                         /*file_count=*/1)));

  // Wait for File Handler dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Get the web contents of the dialog to be able to query
  // `FileHandlerPageElement`.
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Wait for local tasks to be filled in, which indicates the dialog is ready.
  bool dialog_init_complete = false;
  base::Value::List observed_app_ids;
  while (!dialog_init_complete) {
    // It is possible that the `FileHandlerPageElement` element still hasn't
    // been initiated yet. It is completed when the `localTasks` member is
    // non-empty.
    content::EvalJsResult eval_result =
        content::EvalJs(web_contents,
                        "document.querySelector('file-handler-page')"
                        ".localTasks.map(task => task.appId)");
    if (!eval_result.error.empty()) {
      continue;
    }
    observed_app_ids = eval_result.ExtractList().TakeList();
    dialog_init_complete = !observed_app_ids.empty();
  }

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

IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       ShowConnectOneDriveDialog_OpensAndClosesDialog) {
  // Watch for the Connect OneDrive dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Launch the Connect OneDrive dialog.
  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait(browser()->profile());
  ASSERT_TRUE(ShowConnectOneDriveDialog(modal_parent));

  // Wait for the Connect OneDrive dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Check that we have the right dialog page (Connect OneDrive).
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();
  content::EvalJsResult eval_result = content::EvalJs(
      web_contents, "!!document.querySelector('connect-onedrive')");
  ASSERT_TRUE(eval_result.ExtractBool());

  // Click the close button and wait for the dialog to close.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.querySelector('connect-onedrive')"
                              ".$('.cancel-button').click()"));
  watcher.Wait();
}

// Tests that OnDialogComplete() opens the specified fake file task.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       OnDialogCompleteOpensFileTasks) {
  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();
  {
    file_manager::file_tasks::TaskDescriptor default_task;

    auto cloud_open_task = base::WrapRefCounted(
        new CloudOpenTask(profile(), files_, CloudProvider::kGoogleDrive,
                          nullptr, std::move(cloud_open_metrics)));
    cloud_open_task->SetTasksForTest(tasks_);

    for (int selected_task = 0; selected_task < num_tasks_; selected_task++) {
      std::string user_response = base::NumberToString(selected_task);
      // Watch for the selected task to open.
      content::TestNavigationObserver navigation_observer_task(
          (GURL(urls_[selected_task])));
      navigation_observer_task.StartWatchingNewWebContents();

      // Simulate user selecting this task.
      cloud_open_task->OnDialogComplete(user_response);

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

// Tests that OnDialogComplete() doesn't crash when the specified selected task
// doesn't exist.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, OnDialogCompleteNoCrash) {
  auto cloud_open_metrics = std::make_unique<CloudOpenMetrics>(
      CloudProvider::kGoogleDrive, /*file_count=*/1);
  auto cloud_open_metrics_weak_ptr = cloud_open_metrics->GetWeakPtr();
  {
    auto cloud_open_task = base::WrapRefCounted(
        new CloudOpenTask(profile(), files_, CloudProvider::kGoogleDrive,
                          nullptr, std::move(cloud_open_metrics)));
    cloud_open_task->SetTasksForTest(tasks_);

    int out_of_range_task = num_tasks_;
    std::string user_response = base::NumberToString(out_of_range_task);

    // Simulate user selecting a nonexistent selected task.
    cloud_open_task->OnDialogComplete(user_response);
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
    auto fake_provider =
        ash::file_system_provider::FakeExtensionProvider::Create(
            extension_misc::kODFSExtensionId);
    const auto kProviderId = fake_provider->GetId();
    auto* service = file_system_provider::Service::Get(profile());
    service->RegisterProvider(std::move(fake_provider));
    service->MountFileSystem(kProviderId,
                             ash::file_system_provider::MountOptions(
                                 "test-filesystem", "Test FileSystem"));
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

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait(browser()->profile());

  CloudOpenTask::Execute(profile(), files_, CloudProvider::kOneDrive,
                         modal_parent,
                         std::make_unique<CloudOpenMetrics>(
                             CloudProvider::kOneDrive, /*file_count=*/1));

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

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

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  gfx::NativeWindow modal_parent = LaunchFilesAppAndWait(browser()->profile());

  CloudOpenTask::Execute(profile(), files_, CloudProvider::kOneDrive,
                         modal_parent,
                         std::make_unique<CloudOpenMetrics>(
                             CloudProvider::kOneDrive, /*file_count=*/1));

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

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

  auto cloud_open_task = base::WrapRefCounted(
      new CloudOpenTask(profile(), files_, CloudProvider::kOneDrive, nullptr,
                        std::make_unique<CloudOpenMetrics>(
                            CloudProvider::kOneDrive, /*file_count=*/1)));
  mojom::DialogArgsPtr args =
      cloud_open_task->CreateDialogArgs(mojom::DialogPage::kOneDriveSetup);
  // Self-deleted on close.
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), base::DoNothing(),
                            mojom::DialogPage::kOneDriveSetup, false);

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

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

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

  auto cloud_open_task = base::WrapRefCounted(
      new CloudOpenTask(profile(), files_, CloudProvider::kOneDrive, nullptr,
                        std::make_unique<CloudOpenMetrics>(
                            CloudProvider::kOneDrive, /*file_count=*/1)));
  mojom::DialogArgsPtr args =
      cloud_open_task->CreateDialogArgs(mojom::DialogPage::kOneDriveSetup);
  // Self-deleted on close.
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), base::DoNothing(),
                            mojom::DialogPage::kOneDriveSetup, false);

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Open the Welcome Page for the OneDrive set up part of the Setup flow. This
  // will not lead to the Office PWA being set as the default task because there
  // was already a default before running setup.
  dialog->ShowSystemDialog();

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

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

  void SetUpLocalToDriveTask() {
    SetUpMyFiles();

    source_files_.push_back(FilePathToFileSystemURL(
        profile(),
        file_manager::util::GetFileManagerFileSystemContext(profile()),
        my_files_dir_.AppendASCII("file.docx")));

    upload_task_ = base::WrapRefCounted(new ash::cloud_upload::CloudOpenTask(
        profile(), source_files_,
        ash::cloud_upload::CloudProvider::kGoogleDrive, nullptr,
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
        profile(), source_files_,
        ash::cloud_upload::CloudProvider::kGoogleDrive, nullptr,
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
        profile(), source_files_,
        ash::cloud_upload::CloudProvider::kGoogleDrive, nullptr,
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
        profile(), source_files_, ash::cloud_upload::CloudProvider::kOneDrive,
        nullptr,
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
        profile(), source_files_, ash::cloud_upload::CloudProvider::kOneDrive,
        nullptr,
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
        profile(), source_files_, ash::cloud_upload::CloudProvider::kOneDrive,
        nullptr,
        std::make_unique<CloudOpenMetrics>(CloudProvider::kOneDrive,
                                           /*file_count=*/1)));
  }

  bool ShouldShowConfirmationDialog() {
    return upload_task_->ShouldShowConfirmationDialog();
  }

  void OnDialogComplete(const std::string& user_response) {
    upload_task_->OnDialogComplete(user_response);
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
  // Creates mount point for My files and registers local filesystem.
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

  OnDialogComplete(kUserActionUploadToGoogleDrive);

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

  OnDialogComplete(kUserActionUploadToGoogleDrive);

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

  OnDialogComplete(kUserActionUploadToOneDrive);

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

  OnDialogComplete(kUserActionUploadToOneDrive);

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
