// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_

#include <vector>

#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace ash::cloud_upload {

struct ODFSFileSystemAndPath {
  file_system_provider::ProvidedFileSystemInterface* file_system;
  base::FilePath file_path_within_odfs;
};

// The string conversions of ash::cloud_upload::mojom::UserAction.
const char kUserActionCancel[] = "cancel";
const char kUserActionSetUpGoogleDrive[] = "setup-drive";
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

// Return True if feature `kUploadOfficeToCloud` is enabled and is eligible for
// the user, otherwise return False. A user is eligible if they are not managed
// or a Google employee.
bool IsEligibleAndEnabledUploadOfficeToCloud();

// Receive user's dialog response and acts accordingly. The `user_response` is
// either an ash::cloud_upload::mojom::UserAction or the id (position) of the
// task in `tasks` to launch.
void OnDialogComplete(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const std::string& user_response,
    std::vector<::file_manager::file_tasks::TaskDescriptor> tasks);

// Opens the `file_urls` from the `cloud_provider`. Runs setup for Office files
// if it has not been run before. Uploads the files to the cloud if needed.
bool OpenFilesWithCloudProvider(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const CloudProvider cloud_provider);

// Open office files if they are in the correct cloud already.
// Otherwise move the files before opening.
void OpenOrMoveFiles(Profile* profile,
                     const std::vector<storage::FileSystemURL>& file_urls,
                     const CloudProvider cloud_provider);

// Returns True if OneDrive is the selected `cloud_provider` but either ODFS
// is not mounted or the Office PWA is not installed. Returns False otherwise.
bool ShouldFixUpOffice(Profile* profile, const CloudProvider cloud_provider);

bool FileIsOnDriveFS(Profile* profile, const base::FilePath& file_path);

bool FileIsOnODFS(Profile* profile, const FileSystemURL& url);

// Returns True if the file is on the Android OneDrive DocumentsProvider.
bool FileIsOnAndroidOneDrive(Profile* profile, const FileSystemURL& url);

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
  using UploadRequestCallback = base::OnceCallback<void(
      const std::string& user_response,
      std::vector<::file_manager::file_tasks::TaskDescriptor> tasks)>;

  CloudUploadDialog(const CloudUploadDialog&) = delete;
  CloudUploadDialog& operator=(const CloudUploadDialog&) = delete;

  // Creates and shows a new dialog for the cloud upload workflow. If there are
  // local file tasks from `resulting_tasks`, include them in the dialog
  // arguments. These tasks are can be selected by the user to open the files
  // instead of using a cloud provider.
  static void ShowDialog(
      mojom::DialogArgsPtr args,
      const mojom::DialogPage dialog_page,
      UploadRequestCallback uploadCallback,
      std::unique_ptr<::file_manager::file_tasks::ResultingTasks>
          resulting_tasks);

  // Creates and shows a new dialog for the cloud upload workflow by processing
  // the dialog arguments and passing them to `ShowDialog()`. If the
  // `dialog_page` is kFileHandlerDialog, also find the local file tasks
  // and pass them to `ShowDialog()`. Returns true if a new dialog has been
  // effectively created.
  static bool SetUpAndShowDialog(
      Profile* profile,
      const std::vector<storage::FileSystemURL>& file_urls,
      const mojom::DialogPage dialog_page);

  static bool IsODFSMounted(Profile* profile);
  static bool IsOfficeWebAppInstalled(Profile* profile);

  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

 protected:
  CloudUploadDialog(
      mojom::DialogArgsPtr args,
      UploadRequestCallback callback,
      const mojom::DialogPage dialog_page,
      std::vector<::file_manager::file_tasks::TaskDescriptor> tasks);
  ~CloudUploadDialog() override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowCloseButton() const override;
  void GetDialogSize(gfx::Size* size) const override;

 private:
  mojom::DialogArgsPtr dialog_args_;
  UploadRequestCallback callback_;
  mojom::DialogPage dialog_page_;
  std::vector<::file_manager::file_tasks::TaskDescriptor> tasks_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
