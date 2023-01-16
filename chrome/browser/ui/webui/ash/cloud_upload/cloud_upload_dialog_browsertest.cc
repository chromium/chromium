// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <algorithm>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/json/json_parser.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"

namespace ash::cloud_upload {

namespace {

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
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
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

}  // namespace

// Tests the `kFileHandlerDialog` dialog page of the `CloudUploadDialog`.
// Ensures that fake local file tasks are all found and passed to the JS side of
// the dialog - the `FileHandlerPageElement`. Ensures that a local file task
// selected on the JS side gets executed.
class CloudUploadDialogBrowserTest : public InProcessBrowserTest {
 public:
  CloudUploadDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kUploadOfficeToCloud);
  }

  CloudUploadDialogBrowserTest(const CloudUploadDialogBrowserTest&) = delete;
  CloudUploadDialogBrowserTest& operator=(const CloudUploadDialogBrowserTest&) =
      delete;

  Profile* profile() { return browser()->profile(); }

  // Create a test office file for each file extension and store in `files_` and
  // create `n` fake web apps for all office file types.
  void SetUpTasksAndFiles(const std::vector<std::string>& file_extensions,
                          int n) {
    // Create `n` fake web apps for office files with the Doc extension and
    // store the created `urls_` and `tasks_`.
    CreateFakeWebApps(
        profile(), &urls_, &tasks_,
        {doc_file_extension_, ppt_file_extension_, xls_file_extension_},
        {doc_mime_type_, ppt_mime_type_, xls_mime_type_}, n);

    for (const auto& file_extension : file_extensions) {
      base::FilePath file =
          file_manager::util::GetMyFilesFolderForProfile(profile()).AppendASCII(
              "foo" + file_extension);
      GURL url;
      CHECK(file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile(), file, file_manager::util::GetFileManagerURL(), &url));
      files_.push_back(storage::FileSystemURL::CreateForTest(url));
    }
  }

  // Fill in the placeholder from `script_with_placeholder` with the JS command
  // to retrieve the `FileHandlerPageElement`. Return this script wrapped with
  // the send command.
  std::string ScriptJS(const char script_with_placeholder[]) {
    const char send_command[] = "domAutomationController.send(%s)";
    const char dialog[] =
        "document.querySelectorAll('file-handler-"
        "page')[0]";
    std::string script = base::StringPrintf(script_with_placeholder, dialog);
    return base::StringPrintf(send_command, script.c_str());
  }

 protected:
  // The mime type and file_extension must be matching for
  // `CreateFakeWebApps()`.
  std::string doc_file_extension_ = ".doc";
  std::string doc_mime_type_ = "application/msword";
  std::string ppt_file_extension_ = ".ppt";
  std::string ppt_mime_type_ = "application/vnd.ms-powerpoint";
  std::string xls_file_extension_ = ".xls";
  std::string xls_mime_type_ = "application/vnd.ms-excel";
  std::vector<std::string> urls_;
  std::vector<file_manager::file_tasks::TaskDescriptor> tasks_;
  std::vector<storage::FileSystemURL> files_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test which launches a `CloudUploadDialog` which in turn creates a
// `FileHandlerPageElement`. Tests that the `FileHandlerPageElement` observes
// all of the fake file tasks and that a file task can be launched by clicking
// on its button before clicking the open button.
IN_PROC_BROWSER_TEST_F(CloudUploadDialogBrowserTest, OpenFileTaskFromDialog) {
  // Create fake doc and ppt files and 3 fake local file tasks that support all
  // office file types.
  SetUpTasksAndFiles({doc_file_extension_, ppt_file_extension_}, 3);

  // Install QuickOffice.
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());

  // Watch for File Handler dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Check that the Setup flow has never run and so the File
  // Handler dialog will be launched when OpenFilesWithCloudProvider() is
  // called.
  ASSERT_FALSE(file_manager::file_tasks::OfficeSetupComplete(profile()));

  // Launch File Handler dialog.
  ASSERT_TRUE(OpenFilesWithCloudProvider(profile(), files_,
                                         CloudProvider::kGoogleDrive));

  // Wait for File Handler dialog to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  // Get the web contents of the dialog to be able to query
  // `FileHandlerPageElement`.
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUICloudUploadURL);
  ASSERT_TRUE(dialog);
  content::WebUI* webui = dialog->GetWebUIForTest();
  ASSERT_TRUE(webui);
  content::WebContents* web_contents = webui->GetWebContents();
  ASSERT_TRUE(web_contents);

  // Get the `tasks` member from the `FileHandlerPageElement` which are all of
  // the observed local file tasks.
  bool dialog_init_complete = false;
  base::internal::JSONParser parser(base::JSON_PARSE_RFC);
  absl::optional<base::Value> value;
  std::string result;
  while (!dialog_init_complete) {
    // It is possible that the `FileHandlerPageElement` element still hasn't
    // been initiated yet. It is completed when the `tasks` member is non-empty.
    if (!content::ExecuteScriptAndExtractString(
            web_contents,
            ScriptJS("JSON.stringify(%s.tasks.map(task => task.appId))"),
            &result)) {
      continue;
    }
    value = parser.Parse(result);
    ASSERT_TRUE(value->is_list());
    dialog_init_complete = !(value->GetList().empty());
  }

  base::Value::List& observed_app_ids = value->GetList();
// Check QuickOffice was not observed by the dialog.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ASSERT_TRUE(file_manager::file_tasks::IsExtensionInstalled(
      profile(), extension_misc::kQuickOfficeComponentExtensionId));
  ASSERT_LT(PositionInList(observed_app_ids,
                           extension_misc::kQuickOfficeComponentExtensionId),
            0);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Position (in the `tasks_` and `urls_` vector) of the selected file
  // task to be opened. Use this to find the `selected_task_position` and to
  // watch for the appropriate url in `urls_` to open.
  size_t selected_task = 1;
  // Position of the selected task in dialog's tasks array - this is not
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
  file_manager::file_tasks::TaskDescriptor default_task;
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), doc_mime_type_, doc_file_extension_,
      &default_task));

  // Check that there is not a default task for ppt files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), ppt_mime_type_, ppt_file_extension_,
      &default_task));

  // Click the selected task.
  std::string rename_task_id =
      "%s.$('#id" + base::NumberToString(selected_task_position) + "').click()";
  EXPECT_TRUE(content::ExecJs(web_contents, ScriptJS(rename_task_id.c_str())));

  // Click the open button.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              ScriptJS("%s.$('.action-button').click()")));

  // Wait for selected task to open.
  navigation_observer_task.Wait();

  // Check that the Setup flow has been marked complete.
  ASSERT_TRUE(file_manager::file_tasks::OfficeSetupComplete(profile()));

  // Check that the selected task has been made the default for doc files.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), doc_mime_type_, doc_file_extension_,
      &default_task));
  ASSERT_EQ(tasks_[selected_task], default_task);

  // Check that the selected task has been made the default for ppt files.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), ppt_mime_type_, ppt_file_extension_,
      &default_task));
  ASSERT_EQ(tasks_[selected_task], default_task);

  // Check that the selected task has not been made the default for xls files
  // because there was not an xls file selected by the user, even though the
  // task supports xls files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), xls_mime_type_, xls_file_extension_,
      &default_task));
}

// Tests that OnDialogComplete() opens the specified fake file task.
IN_PROC_BROWSER_TEST_F(CloudUploadDialogBrowserTest,
                       OnDialogCompleteOpensFileTasks) {
  file_manager::file_tasks::TaskDescriptor default_task;
  int num_tasks = 3;
  SetUpTasksAndFiles({xls_file_extension_}, num_tasks);

  for (int selected_task = 0; selected_task < num_tasks; selected_task++) {
    std::string user_response = base::NumberToString(selected_task);
    // Watch for the selected task to open.
    content::TestNavigationObserver navigation_observer_task(
        (GURL(urls_[selected_task])));
    navigation_observer_task.StartWatchingNewWebContents();

    std::vector<file_manager::file_tasks::TaskDescriptor> tasks = tasks_;

    // Simulate user selecting this task.
    OnDialogComplete(profile(), files_, user_response, std::move(tasks));

    // Wait for the selected task to open.
    navigation_observer_task.Wait();

    // Check that the selected task has been made the default.
    ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
        *profile()->GetPrefs(), xls_mime_type_, xls_file_extension_,
        &default_task));
    ASSERT_EQ(tasks_[selected_task], default_task);
  }
}

// Tests that OnDialogComplete() doesn't crash when the specified selected task
// doesn't exist.
IN_PROC_BROWSER_TEST_F(CloudUploadDialogBrowserTest, OnDialogCompleteNoCrash) {
  int num_tasks = 3;
  SetUpTasksAndFiles({ppt_file_extension_}, num_tasks);

  int out_of_range_task = 3;
  std::string user_response = base::NumberToString(out_of_range_task);

  // Simulate user selecting a nonexistent selected task.
  OnDialogComplete(profile(), files_, user_response, std::move(tasks_));
}
}  // namespace ash::cloud_upload
