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
#include "base/json/json_parser.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
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

// The mime type and file_extension must be matching for
// `CreateFakeWebApps()`.
const char kDocFileExtension[] = ".doc";
const char kDocMimeType[] = "application/msword";
const char kPptFileExtension[] = ".ppt";
const char kPptMimeType[] = "application/vnd.ms-powerpoint";
const char kXlsFileExtension[] = ".xls";
const char kXlsMimeType[] = "application/vnd.ms-excel";

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

// Fill in the placeholder from `script_with_placeholder` with the JS command
// to retrieve the HTML `element`. Return the resulting JS script.
std::string ScriptFillPlaceholder(const char script_with_placeholder[],
                                  std::string element) {
  const char element_with_placeholder[] = "document.querySelectorAll('%s')[0]";
  std::string element_script =
      base::StringPrintf(element_with_placeholder, element.c_str());
  return base::StringPrintf(script_with_placeholder, element_script.c_str());
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

}  // namespace

// Tests the `kFileHandlerDialog` dialog page of the `CloudUploadDialog`.
// Ensures that fake local file tasks are all found and passed to the JS side of
// the dialog - the `FileHandlerPageElement`. Ensures that a local file task
// selected on the JS side gets executed.
class FileHandlerDialogBrowserTest : public InProcessBrowserTest {
 public:
  FileHandlerDialogBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kUploadOfficeToCloud);
  }

  FileHandlerDialogBrowserTest(const FileHandlerDialogBrowserTest&) = delete;
  FileHandlerDialogBrowserTest& operator=(const FileHandlerDialogBrowserTest&) =
      delete;

  Profile* profile() { return browser()->profile(); }

  // Create a test office file for each file extension and store in `files_` and
  // create `n` fake web apps for all office file types.
  void SetUpTasksAndFiles(const std::vector<std::string>& file_extensions,
                          int n) {
    // Create `n` fake web apps for office files with the Doc extension and
    // store the created `urls_` and `tasks_`.
    CreateFakeWebApps(profile(), &urls_, &tasks_,
                      {kDocFileExtension, kPptFileExtension, kXlsFileExtension},
                      {kDocMimeType, kPptMimeType, kXlsMimeType}, n);

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

 protected:
  // Use a non-managed user in this browser test to ensure
  // |IsEligibleAndEnabledUploadOfficeToCloud| returns the result of
  // |IsUploadOfficeToCloudEnabled|.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpCommandLineForNonManagedUser(command_line);
  }

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
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, OpenFileTaskFromDialog) {
  // Create fake doc and ppt files and 3 fake local file tasks that support all
  // office file types.
  SetUpTasksAndFiles({kDocFileExtension, kPptFileExtension}, 3);

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
  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

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
            base::StringPrintf(
                "domAutomationController.send(%s)",
                ScriptFillPlaceholder(
                    "JSON.stringify(%s.tasks.map(task => task.appId))",
                    "file-handler-page")
                    .c_str()),
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
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension, &default_task));

  // Check that there is not a default task for ppt files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kPptMimeType, kPptFileExtension, &default_task));

  // Expand local tasks accordion.
  std::string expand_local_tasks = "%s.$('#accordion').click()";
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(expand_local_tasks.c_str(), "file-handler-page")));

  // Click the selected task.
  std::string rename_task_id =
      "%s.$('#id" + base::NumberToString(selected_task_position) + "').click()";
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(rename_task_id.c_str(), "file-handler-page")));

  // Click the open button.
  EXPECT_TRUE(content::ExecJs(
      web_contents, ScriptFillPlaceholder("%s.$('.action-button').click()",
                                          "file-handler-page")));

  // Wait for selected task to open.
  navigation_observer_task.Wait();

  // Check that the Setup flow has been marked complete.
  ASSERT_TRUE(file_manager::file_tasks::OfficeSetupComplete(profile()));

  // Check that the selected task has been made the default for doc files.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension, &default_task));
  ASSERT_EQ(tasks_[selected_task], default_task);

  // Check that the selected task has been made the default for ppt files.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kPptMimeType, kPptFileExtension, &default_task));
  ASSERT_EQ(tasks_[selected_task], default_task);

  // Check that the selected task has not been made the default for xls files
  // because there was not an xls file selected by the user, even though the
  // task supports xls files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kXlsMimeType, kXlsFileExtension, &default_task));
}

// Tests that OnDialogComplete() opens the specified fake file task.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest,
                       OnDialogCompleteOpensFileTasks) {
  file_manager::file_tasks::TaskDescriptor default_task;
  int num_tasks = 3;
  SetUpTasksAndFiles({kXlsFileExtension}, num_tasks);

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
        *profile()->GetPrefs(), kXlsMimeType, kXlsFileExtension,
        &default_task));
    ASSERT_EQ(tasks_[selected_task], default_task);
  }
}

// Tests that OnDialogComplete() doesn't crash when the specified selected task
// doesn't exist.
IN_PROC_BROWSER_TEST_F(FileHandlerDialogBrowserTest, OnDialogCompleteNoCrash) {
  int num_tasks = 3;
  SetUpTasksAndFiles({kPptFileExtension}, num_tasks);

  int out_of_range_task = 3;
  std::string user_response = base::NumberToString(out_of_range_task);

  // Simulate user selecting a nonexistent selected task.
  OnDialogComplete(profile(), files_, user_response, std::move(tasks_));
}

// Tests the Fixup flow. Ensures that it is run when the conditions are met: the
// Setup flow has completed, ODFS is not mounted or the Office PWA is not
// installed and OneDrive is selected as the cloud provider. Ensures that it
// cannot change the default task set.
class FixUpFlowBrowserTest : public InProcessBrowserTest {
 public:
  FixUpFlowBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kUploadOfficeToCloud);
  }

  FixUpFlowBrowserTest(const FixUpFlowBrowserTest&) = delete;
  FixUpFlowBrowserTest& operator=(const FixUpFlowBrowserTest&) = delete;

  Profile* profile() { return browser()->profile(); }

  // Add a doc test file.
  void SetUpFiles() {
    base::FilePath file =
        file_manager::util::GetMyFilesFolderForProfile(profile()).AppendASCII(
            "foo.doc");
    GURL url;
    CHECK(file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile(), file, file_manager::util::GetFileManagerURL(), &url));
    files_.push_back(storage::FileSystemURL::CreateForTest(url));
  }

  void AddFakeODFS() {
    auto fake_provider =
        ash::file_system_provider::FakeExtensionProvider::Create(
            file_manager::file_tasks::kODFSExtensionId);
    const auto kProviderId = fake_provider->GetId();
    auto* service = file_system_provider::Service::Get(profile());
    service->RegisterProvider(std::move(fake_provider));
    service->MountFileSystem(kProviderId,
                             ash::file_system_provider::MountOptions(
                                 "test-filesystem", "Test FileSystem"));
  }

  void AddFakeOfficePWA() {
    file_manager::test::AddFakeWebApp(
        web_app::kMicrosoftOfficeAppId, kDocMimeType, kDocFileExtension, "",
        true, apps::AppServiceProxyFactory::GetForProfile(profile()));
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

// Tests that the Fixup flow is entered when OneDrive is selected as the cloud
// provider but ODFS is not mounted and the Setup flow has already completed.
// Checks that the ODFS Sign In Page is reachable.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest, FixUpFlowWhenODFSNotMounted) {
  // Set Setup flow as complete.
  file_manager::file_tasks::SetOfficeSetupComplete(profile(), true);

  SetUpFiles();
  AddFakeOfficePWA();

  // ODFS is not mounted, expect that the Fixup flow will need to run.
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  OpenFilesWithCloudProvider(profile(), files_, CloudProvider::kOneDrive);

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(
          "%s.$('welcome-page').querySelector('.action-button').click()",
          "cloud-upload"))) {
  }

  // Wait for the ODFS Sign In Page.
  while (!content::ExecJs(
      web_contents, ScriptFillPlaceholder(
                        "%s.$('sign-in-page').querySelector('.action-button')",
                        "cloud-upload"))) {
  }
}

// Tests that the Fixup flow is entered when OneDrive is selected as the cloud
// provider but the Office PWA is not installed and the Setup flow has already
// completed. Checks that the Office PWA Install Page is reachable.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest,
                       FixUpFlowWhenOfficePWANotInstalled) {
  // Set Setup flow as complete.
  file_manager::file_tasks::SetOfficeSetupComplete(profile(), true);

  SetUpFiles();
  AddFakeODFS();

  // Office PWA is not installed, expect that the Fixup flow will need to run.
  ASSERT_TRUE(ShouldFixUpOffice(profile(), CloudProvider::kOneDrive));

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  OpenFilesWithCloudProvider(profile(), files_, CloudProvider::kOneDrive);

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(
          "%s.$('welcome-page').querySelector('.action-button').click()",
          "cloud-upload"))) {
  }

  // Wait for the Office PWA Install Page, this script will fail until the page
  // exists.
  while (!content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(
          "%s.$('office-pwa-install-page').querySelector('.action-button')",
          "cloud-upload"))) {
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
  // Set Setup flow as incomplete.
  file_manager::file_tasks::SetOfficeSetupComplete(profile(), false);

  // Add a doc test file.
  SetUpFiles();
  AddFakeODFS();
  AddFakeOfficePWA();

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Check that there is not a default task for doc files.
  file_manager::file_tasks::TaskDescriptor default_task;
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension, &default_task));

  // Open the Welcome Page for the OneDrive set up part of the Setup flow. This
  // will lead to the Office PWA being set as the default task.
  CloudUploadDialog::SetUpAndShowDialog(profile(), files_,
                                        mojom::DialogPage::kOneDriveSetup);

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(
          "%s.$('welcome-page').querySelector('.action-button').click()",
          "cloud-upload"))) {
  }

  // Click through the Upload Page.
  while (!content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(
          "%s.$('upload-page').querySelector('.action-button').click()",
          "cloud-upload"))) {
  }

  // Check that the Office PWA has been made the default for doc files.
  ASSERT_TRUE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension, &default_task));
  ASSERT_TRUE(IsOpenInOfficeTask(default_task));
}

// Test that entering and completing the Setup flow from the OneDrive Set Up
// point does not change the default task set when the Setup has been run
// before. This is to test that when the Fixup flow runs, the default task does
// not change.
IN_PROC_BROWSER_TEST_F(FixUpFlowBrowserTest,
                       OneDriveSetUpDoesNotChangeDefaultTaskWhenSetUpComplete) {
  // Set Setup flow as complete.
  file_manager::file_tasks::SetOfficeSetupComplete(profile(), true);

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

  // Watch for OneDrive Setup dialog URL chrome://cloud-upload.
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUICloudUploadURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  // Check that there is not a default task for doc files.
  file_manager::file_tasks::TaskDescriptor default_task;
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension, &default_task));

  // Open the Welcome Page for the OneDrive set up part of the Setup flow. This
  // will not lead to the Office PWA being set as the default task because the
  // Setup flow has already been completed.
  CloudUploadDialog::SetUpAndShowDialog(profile(), files_,
                                        mojom::DialogPage::kOneDriveSetup);

  // Wait for Welcome Page to open at chrome://cloud-upload.
  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromCloudUploadDialog();

  // Click through the Welcome Page.
  while (!content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(
          "%s.$('welcome-page').querySelector('.action-button').click()",
          "cloud-upload"))) {
  }

  // Click through the Upload Page.
  while (!content::ExecJs(
      web_contents,
      ScriptFillPlaceholder(
          "%s.$('upload-page').querySelector('.action-button').click()",
          "cloud-upload"))) {
  }

  // Check that there is still not a default task for doc files.
  ASSERT_FALSE(file_manager::file_tasks::GetDefaultTaskFromPrefs(
      *profile()->GetPrefs(), kDocMimeType, kDocFileExtension, &default_task));
}
}  // namespace ash::cloud_upload

void NonManagedUserWebUIBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ash::cloud_upload::SetUpCommandLineForNonManagedUser(command_line);
}
