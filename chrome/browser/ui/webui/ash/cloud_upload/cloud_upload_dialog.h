// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace extensions::app_file_handler_util {
class MimeTypeCollector;
}  // namespace extensions::app_file_handler_util

namespace file_manager::file_tasks {
FORWARD_DECLARE_TEST(DriveTest, OpenFileInDrive);
FORWARD_DECLARE_TEST(OneDriveTest, OpenFileFromODFS);
FORWARD_DECLARE_TEST(OneDriveTest, OpenFileNotFromODFS);
FORWARD_DECLARE_TEST(OneDriveTest, OpenFileFromAndroidOneDriveViaODFS);
FORWARD_DECLARE_TEST(OneDriveTest,
                     FailToOpenFileFromAndroidOneDriveViaODFSDiffEmail);
FORWARD_DECLARE_TEST(OneDriveTest, FailToOpenFileFromAndroidOneDriveNotOnODFS);
FORWARD_DECLARE_TEST(
    OneDriveTest,
    FailToOpenFileFromAndroidOneDriveDirectoryNotAccessibleToODFS);
}  // namespace file_manager::file_tasks

namespace ash::cloud_upload {

struct ODFSFileSystemAndPath {
  file_system_provider::ProvidedFileSystemInterface* file_system;
  base::FilePath file_path_within_odfs;
};

// The string conversions of ash::cloud_upload::mojom::UserAction.
const char kUserActionCancel[] = "cancel";
const char kUserActionSetUpOneDrive[] = "setup-onedrive";
const char kUserActionUploadToGoogleDrive[] = "upload-drive";
const char kUserActionUploadToOneDrive[] = "upload-onedrive";
const char kUserActionConfirmOrUploadToGoogleDrive[] =
    "confirm-or-upload-google-drive";
const char kUserActionConfirmOrUploadToOneDrive[] =
    "confirm-or-upload-onedrive";

// Custom action ids passed from ODFS.
const char kOneDriveUrlActionId[] = "HIDDEN_ONEDRIVE_URL";
const char kUserEmailActionId[] = "HIDDEN_ONEDRIVE_USER_EMAIL";

// Either OneDrive for the Office PWA or Drive for Drive Web editing.
enum class CloudProvider {
  kGoogleDrive,
  kOneDrive,
};

class CloudUploadDialog;

// The business logic for running setup, moving files to a cloud provider, and
// opening files on cloud providers. Spawns instances of `CloudUploadDialog` if
// necessary to run setup or get confirmation from the user.
class CloudOpenTask : public BrowserListObserver,
                      public base::RefCounted<CloudOpenTask> {
 public:
  CloudOpenTask(const CloudOpenTask&) = delete;
  CloudOpenTask& operator=(const CloudOpenTask&) = delete;

  // This is the main public entry-point to the functionality in this file.
  // Opens the `file_urls` from the `cloud_provider`. Runs setup for Office
  // files if it has not been run before. Uploads the files to the cloud if
  // needed. Returns false if `file_urls` was empty or the setup/upload process
  // failed before any async step started. `CloudOpenTask` is refcounted and
  // lives until the task is complete or has failed, via bound `this` pointers
  // in the closures used for async steps.
  static bool Execute(Profile* profile,
                      const std::vector<storage::FileSystemURL>& file_urls,
                      const CloudProvider cloud_provider,
                      gfx::NativeWindow modal_parent);

  // Set the local tasks that are passed to the File Handler dialog. Normally
  // tasks are calculated internally by this class before displaying this
  // dialog. Some tests don't display the dialog, but only test post-dialog
  // logic.
  void SetTasksForTest(
      const std::vector<::file_manager::file_tasks::TaskDescriptor>& tasks);

  // BrowserListObserver implementation.
  // This is called when we are waiting for a new Files app window to be
  // launched, to be used as the modal parent. This is triggered by
  // ShowDialog().
  void OnBrowserAdded(Browser* browser) override;

  FRIEND_TEST_ALL_PREFIXES(FileHandlerDialogBrowserTest,
                           OnDialogCompleteOpensFileTasks);
  FRIEND_TEST_ALL_PREFIXES(FileHandlerDialogBrowserTest,
                           OnDialogCompleteNoCrash);
  FRIEND_TEST_ALL_PREFIXES(FixUpFlowBrowserTest,
                           OneDriveSetUpChangesDefaultTaskWhenSetUpIncomplete);
  FRIEND_TEST_ALL_PREFIXES(
      FixUpFlowBrowserTest,
      OneDriveSetUpDoesNotChangeDefaultTaskWhenSetUpComplete);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::DriveTest,
                           OpenFileInDrive);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           OpenFileFromODFS);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           OpenFileNotFromODFS);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           OpenFileFromAndroidOneDriveViaODFS);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           FailToOpenFileFromAndroidOneDriveViaODFSDiffEmail);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           FailToOpenFileFromAndroidOneDriveNotOnODFS);
  FRIEND_TEST_ALL_PREFIXES(
      ::file_manager::file_tasks::OneDriveTest,
      FailToOpenFileFromAndroidOneDriveDirectoryNotAccessibleToODFS);

 private:
  friend class RefCounted<CloudOpenTask>;  // Allow destruction by RefCounted<>.
  friend class CloudOpenTaskBrowserTest;

  CloudOpenTask(Profile* profile,
                std::vector<storage::FileSystemURL> file_urls,
                const CloudProvider cloud_provider,
                gfx::NativeWindow modal_parent);

  ~CloudOpenTask() override;

  // See the .cc implementation for comments on private methods.
  bool ExecuteInternal();
  void OpenOrMoveFiles();
  void OpenAlreadyHostedDriveUrls();
  void OpenODFSUrls();
  void OpenAndroidOneDriveUrlsIfAccountMatchedODFS();
  void CheckEmailAndOpenURLs(const std::string& android_onedrive_email,
                             const file_system_provider::Actions& actions,
                             base::File::Error result);

  bool ShouldShowConfirmationDialog();
  void ConfirmMoveOrStartUpload();
  void StartUpload();

  void FinishedDriveUpload(const GURL& url);
  void FinishedOneDriveUpload(base::WeakPtr<Profile> profile_weak_ptr,
                              const storage::FileSystemURL& url);

  bool InitAndShowDialog(mojom::DialogPage dialog_page);
  mojom::DialogArgsPtr CreateDialogArgs(mojom::DialogPage dialog_page);
  void ShowDialog(mojom::DialogArgsPtr args,
                  const mojom::DialogPage dialog_page,
                  std::unique_ptr<::file_manager::file_tasks::ResultingTasks>
                      resulting_tasks);
  void SetTaskArgs(mojom::DialogArgsPtr& args,
                   std::unique_ptr<::file_manager::file_tasks::ResultingTasks>
                       resulting_tasks);

  void OnDialogComplete(const std::string& user_response);
  void LaunchLocalFileTask(const std::string& string_task_position);

  void LocalTaskExecuted(
      const ::file_manager::file_tasks::TaskDescriptor& task,
      extensions::api::file_manager_private::TaskResult result,
      std::string error_message);

  void FindTasksForDialog(::file_manager::file_tasks::FindTasksCallback
                              find_all_types_of_tasks_callback);
  void ConstructEntriesAndFindTasks(
      const std::vector<base::FilePath>& file_paths,
      const std::vector<GURL>& gurls,
      std::unique_ptr<extensions::app_file_handler_util::MimeTypeCollector>
          mime_collector,
      ::file_manager::file_tasks::FindTasksCallback
          find_all_types_of_tasks_callback,
      std::unique_ptr<std::vector<std::string>> mime_types);

  raw_ptr<Profile, ExperimentalAsh> profile_;
  std::vector<storage::FileSystemURL> file_urls_;
  CloudProvider cloud_provider_;
  gfx::NativeWindow modal_parent_;
  std::vector<::file_manager::file_tasks::TaskDescriptor> local_tasks_;
  size_t pending_uploads_ = 0;
  CloudUploadDialog* pending_dialog_ = nullptr;
};

// Return True if feature `kUploadOfficeToCloud` is enabled and is eligible for
// the user of the |profile|. A user is eligible if they are not managed.
bool IsEligibleAndEnabledUploadOfficeToCloud(Profile* profile);

// Returns True if OneDrive is the selected `cloud_provider` but either ODFS
// is not mounted or the Office PWA is not installed. Returns False otherwise.
bool ShouldFixUpOffice(Profile* profile, const CloudProvider cloud_provider);

// Returns True if the url is on ODFS.
bool UrlIsOnODFS(Profile* profile, const FileSystemURL& url);

// Returns True if the url is on the Android OneDrive DocumentsProvider.
bool UrlIsOnAndroidOneDrive(Profile* profile, const FileSystemURL& url);

// Return the email from the Root Document Id of the Android OneDrive
// DocumentsProvider.
absl::optional<std::string> GetEmailFromAndroidOneDriveRootDoc(
    const std::string& root_document_id);

// If the Microsoft account logged into the Android OneDrive matches the account
// logged into ODFS, open office files from ODFS that were originally selected
// from Android OneDrive. Open the files in the MS 365 PWA. Fails if the Android
// OneDrive URLS cannot be converted to valid ODFS file paths.
void OpenAndroidOneDriveUrlsIfAccountMatchedODFS(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& android_onedrive_urls);

// Converts the |android_onedrive_file_url| for a file in OneDrive to the
// equivalent ODFS file path which is then parsed to detect the corresponding
// ODFS ProvidedFileSystemInterface and relative file path. There may or may not
// exist a file for the returned relative file path. The conversion can be done
// for files in OneDrive that can be accessed via Android OneDrive or ODFS.
// These are the users' own files - in the Android OneDrive "Files" directory.
// Fails if an equivalent ODFS file path can't be constructed.
absl::optional<ODFSFileSystemAndPath> AndroidOneDriveUrlToODFS(
    Profile* profile,
    const FileSystemURL& android_onedrive_file_url);

// Defines the web dialog used to help users upload Office files to the cloud.
class CloudUploadDialog : public SystemWebDialogDelegate {
 public:
  using UploadRequestCallback =
      base::OnceCallback<void(const std::string& user_response)>;

  CloudUploadDialog(const CloudUploadDialog&) = delete;
  CloudUploadDialog& operator=(const CloudUploadDialog&) = delete;

  CloudUploadDialog(mojom::DialogArgsPtr args,
                    UploadRequestCallback callback,
                    const mojom::DialogPage dialog_page,
                    bool office_move_confirmation_shown);

  // Request ODFS be mounted. If there is an existing mount, ODFS will unmount
  // that one after authentication of the new mount.
  static void RequestODFSMount(
      Profile* profile,
      file_system_provider::RequestMountCallback callback);
  static bool IsODFSMounted(Profile* profile);
  static bool IsOfficeWebAppInstalled(Profile* profile);

  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

 protected:
  ~CloudUploadDialog() override;
  ui::ModalType GetDialogModalType() const override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowCloseButton() const override;
  void GetDialogSize(gfx::Size* size) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FixUpFlowBrowserTest,
                           OneDriveSetUpChangesDefaultTaskWhenSetUpIncomplete);
  FRIEND_TEST_ALL_PREFIXES(
      FixUpFlowBrowserTest,
      OneDriveSetUpDoesNotChangeDefaultTaskWhenSetUpComplete);

  mojom::DialogArgsPtr dialog_args_;
  UploadRequestCallback callback_;
  mojom::DialogPage dialog_page_;
  size_t num_local_tasks_;
  bool office_move_confirmation_shown_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
