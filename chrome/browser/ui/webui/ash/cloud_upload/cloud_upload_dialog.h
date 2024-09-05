// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_

#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/drive/file_errors.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace extensions::app_file_handler_util {
class MimeTypeCollector;
}  // namespace extensions::app_file_handler_util

namespace file_manager::file_tasks {
FORWARD_DECLARE_TEST(DriveTest, OpenFileInDrive);
FORWARD_DECLARE_TEST(OneDriveTest, OpenFileFromODFS);
FORWARD_DECLARE_TEST(OneDriveTest, OpenFileNotFromODFS);
FORWARD_DECLARE_TEST(OneDriveTest,
                     FailToOpenFileFromODFSReauthenticationRequired);
FORWARD_DECLARE_TEST(OneDriveTest, FailToOpenFileFromODFSOtherAccessError);
FORWARD_DECLARE_TEST(OneDriveTest, OpenFileFromAndroidOneDriveViaODFS);
FORWARD_DECLARE_TEST(OneDriveTest,
                     OpenFileFromAndroidOneDriveViaODFSDiffCaseEmail);
FORWARD_DECLARE_TEST(OneDriveTest,
                     FailToOpenFileFromAndroidOneDriveViaODFSDiffEmail);
FORWARD_DECLARE_TEST(OneDriveTest, FailToOpenFileFromAndroidOneDriveNotOnODFS);
FORWARD_DECLARE_TEST(
    OneDriveTest,
    FailToOpenFileFromAndroidOneDriveDirectoryNotAccessibleToODFS);
}  // namespace file_manager::file_tasks

namespace ash::cloud_upload {

struct ODFSFileSystemAndPath {
  raw_ptr<file_system_provider::ProvidedFileSystemInterface> file_system;
  base::FilePath file_path_within_odfs;
};

// The string conversions of ash::cloud_upload::mojom::UserAction.
constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionCancelGoogleDrive[] = "cancel-drive";
constexpr char kUserActionCancelOneDrive[] = "cancel-onedrive";
constexpr char kUserActionSetUpOneDrive[] = "setup-onedrive";
constexpr char kUserActionUploadToGoogleDrive[] = "upload-drive";
constexpr char kUserActionUploadToOneDrive[] = "upload-onedrive";
constexpr char kUserActionConfirmOrUploadToGoogleDrive[] =
    "confirm-or-upload-google-drive";
constexpr char kUserActionConfirmOrUploadToOneDrive[] =
    "confirm-or-upload-onedrive";

// Options for which setup or move confirmation sub-page/flow we want to show.
enum class SetupOrMoveDialogPage {
  // The user can choose between apps for handling office files.
  kFileHandlerDialog,
  // Set up OneDrive (multi-page).
  kOneDriveSetup,
  // Confirm that the user wants to move the file to OneDrive.
  kMoveConfirmationOneDrive,
  // Confirm that the user wants to move the file to Google Drive.
  kMoveConfirmationGoogleDrive,
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
                      const ::file_manager::file_tasks::TaskDescriptor& task,
                      const CloudProvider cloud_provider,
                      std::unique_ptr<CloudOpenMetrics> cloud_open_metrics);

  // Set the local tasks that are passed to the File Handler dialog. Normally
  // tasks are calculated internally by this class before displaying this
  // dialog. Some tests don't display the dialog, but only test post-dialog
  // logic.
  void SetTasksForTest(
      const std::vector<::file_manager::file_tasks::TaskDescriptor>& tasks);

  // BrowserListObserver implementation.
  // Use this to check if a new Files app window has been launched when there
  // wasn't already one to be used as the modal parent. This is triggered by
  // ShowDialog().
  void OnBrowserAdded(Browser* browser) override;
  // Use this to check if the Files app window the dialog is modal to has
  // closed.
  void OnBrowserClosing(Browser* browser) override;

  FRIEND_TEST_ALL_PREFIXES(FileHandlerDialogBrowserTest,
                           OnSetupDialogCompleteOpensFileTasks);
  FRIEND_TEST_ALL_PREFIXES(FileHandlerDialogBrowserTest,
                           OnSetupDialogCompleteNoCrash);
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
                           FailToOpenFileFromODFSReauthenticationRequired);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           FailToOpenFileFromODFSOtherAccessError);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           OpenFileFromAndroidOneDriveViaODFS);
  FRIEND_TEST_ALL_PREFIXES(::file_manager::file_tasks::OneDriveTest,
                           OpenFileFromAndroidOneDriveViaODFSDiffCaseEmail);
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
                const ::file_manager::file_tasks::TaskDescriptor& task,
                const CloudProvider cloud_provider,
                std::unique_ptr<CloudOpenMetrics> cloud_open_metrics);

  ~CloudOpenTask() override;

  // See the .cc implementation for comments on private methods.
  bool ExecuteInternal();
  bool MaybeRunFixupFlow();
  bool OpenOrMoveFiles();
  void OpenAlreadyHostedDriveUrls();
  void OnGoogleDriveGetMetadata(drive::FileError error,
                                drivefs::mojom::FileMetadataPtr metadata);
  void OpenUploadedDriveUrl(const GURL& url,
                            const OfficeTaskResult task_result);
  void OpenODFSUrls(const OfficeTaskResult task_result_uma);
  void CheckEmailAndOpenAndroidOneDriveURLs(
      base::expected<cloud_upload::ODFSMetadata, base::File::Error>
          metadata_or_error);
  void OpenAndroidOneDriveUrl(
      const storage::FileSystemURL& android_onedrive_url);

  bool ShouldShowConfirmationDialog();
  bool ConfirmMoveOrStartUpload();
  void StartUpload();
  void StartNextGoogleDriveUpload();
  void StartNextOneDriveUpload();

  // Callbacks from `DriveUploadHandler` and `OneDriveUploadHandler`. URL passed
  // to these callbacks will be `std::nullopt` and size will be 0 if upload
  // fails.
  void FinishedDriveUpload(OfficeTaskResult task_result,
                           std::optional<GURL> url,
                           int64_t size);
  void FinishedOneDriveUpload(base::WeakPtr<Profile> profile_weak_ptr,
                              OfficeTaskResult task_result,
                              std::optional<storage::FileSystemURL> url,
                              int64_t size);

  void LogGoogleDriveOpenResultUMA(OfficeTaskResult success_task_result,
                                   OfficeDriveOpenErrors open_result);
  void LogOneDriveOpenResultUMA(OfficeTaskResult success_task_result,
                                OfficeOneDriveOpenErrors open_result);

  bool InitAndShowSetupOrMoveDialog(SetupOrMoveDialogPage dialog_page);
  mojom::DialogArgsPtr CreateDialogArgs(SetupOrMoveDialogPage dialog_page);
  void ShowDialog(SetupOrMoveDialogPage dialog_page,
                  mojom::DialogArgsPtr args,
                  std::unique_ptr<::file_manager::file_tasks::ResultingTasks>
                      resulting_tasks);
  void SetTaskArgs(mojom::DialogArgsPtr& args,
                   std::unique_ptr<::file_manager::file_tasks::ResultingTasks>
                       resulting_tasks);

  void OnSetupDialogComplete(const std::string& user_response);
  void OnMoveConfirmationComplete(const std::string& user_response);
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
  void RecordUploadLatencyUMA();

  raw_ptr<Profile> profile_;
  std::vector<storage::FileSystemURL> file_urls_;
  // File being currently uploaded.
  size_t file_urls_idx_ = 0;
  const ::file_manager::file_tasks::TaskDescriptor task_;
  CloudProvider cloud_provider_;
  std::unique_ptr<CloudOpenMetrics> cloud_open_metrics_;
  std::unique_ptr<DriveUploadHandler> drive_upload_handler_;
  std::unique_ptr<OneDriveUploadHandler> one_drive_upload_handler_;
  std::vector<::file_manager::file_tasks::TaskDescriptor> local_tasks_;
  raw_ptr<CloudUploadDialog> pending_dialog_ = nullptr;
  base::ElapsedTimer upload_timer_;
  int64_t upload_total_size_ = 0;
  // True when there is at least one upload error.
  bool has_upload_errors_ = false;
  OfficeFilesTransferRequired transfer_required_ =
      OfficeFilesTransferRequired::kNotRequired;
  bool need_new_files_app_ = false;
  raw_ptr<Browser> files_app_browser_;
  bool files_app_closed_ = false;
};

// Returns True if OneDrive is the selected `cloud_provider` but either ODFS
// is not mounted or the Office PWA is not installed. Returns False otherwise.
bool ShouldFixUpOffice(Profile* profile, const CloudProvider cloud_provider);

// Returns True if the url is on the Android OneDrive DocumentsProvider.
bool UrlIsOnAndroidOneDrive(Profile* profile, const FileSystemURL& url);

// Return the email from the Root Document Id of the Android OneDrive
// DocumentsProvider.
std::optional<std::string> GetEmailFromAndroidOneDriveRootDoc(
    const std::string& root_document_id);

// Converts the |android_onedrive_file_url| for a file in OneDrive to the
// equivalent ODFS file path which is then parsed to detect the corresponding
// ODFS ProvidedFileSystemInterface and relative file path. There may or may not
// exist a file for the returned relative file path. The conversion can be done
// for files in OneDrive that can be accessed via Android OneDrive or ODFS.
// These are the users' own files - in the Android OneDrive "Files" directory.
// Fails if an equivalent ODFS file path can't be constructed.
std::optional<ODFSFileSystemAndPath> AndroidOneDriveUrlToODFS(
    Profile* profile,
    const FileSystemURL& android_onedrive_file_url);

// Launches the 'Connect OneDrive' dialog which is triggered from the Services
// menu in Files app. This is a simplified version of setup where we just
// connect to OneDrive. There is no 'done' callback because files app doesn't
// need it.
bool ShowConnectOneDriveDialog(gfx::NativeWindow modal_parent);

// Launches the setup flow to set up opening Office files in Microsoft 365.
void LaunchMicrosoft365Setup(Profile* profile, gfx::NativeWindow modal_parent);

// Defines the web dialog used to help users upload Office files to the cloud.
class CloudUploadDialog : public SystemWebDialogDelegate {
 public:
  using UploadRequestCallback =
      base::OnceCallback<void(const std::string& user_response)>;

  CloudUploadDialog(const CloudUploadDialog&) = delete;
  CloudUploadDialog& operator=(const CloudUploadDialog&) = delete;

  CloudUploadDialog(mojom::DialogArgsPtr args,
                    UploadRequestCallback callback,
                    bool office_move_confirmation_shown);

  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

 protected:
  ~CloudUploadDialog() override;
  ui::mojom::ModalType GetDialogModalType() const override;
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
  bool office_move_confirmation_shown_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
