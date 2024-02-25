// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_PAGE_HANDLER_H_

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash::cloud_upload {

// Handles communication from the chrome://cloud-upload renderer process to
// the browser process exposing various methods for the JS to invoke.
class CloudUploadPageHandler : public mojom::PageHandler {
 public:
  using RespondWithUserActionAndCloseCallback =
      base::OnceCallback<void(mojom::UserAction action)>;
  using RespondWithLocalTaskAndCloseCallback =
      base::OnceCallback<void(int task_position)>;
  CloudUploadPageHandler(
      content::WebUI* web_ui,
      Profile* profile,
      mojom::DialogArgsPtr args,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
      RespondWithUserActionAndCloseCallback user_action_callback,
      RespondWithLocalTaskAndCloseCallback local_task_callback);

  CloudUploadPageHandler(const CloudUploadPageHandler&) = delete;
  CloudUploadPageHandler& operator=(const CloudUploadPageHandler&) = delete;

  ~CloudUploadPageHandler() override;

  void OnMountResponse(
      CloudUploadPageHandler::SignInToOneDriveCallback callback,
      base::File::Error result);

  // mojom::PageHandler:
  void GetDialogArgs(GetDialogArgsCallback callback) override;
  void IsOfficeWebAppInstalled(
      IsOfficeWebAppInstalledCallback callback) override;
  void InstallOfficeWebApp(InstallOfficeWebAppCallback callback) override;
  void IsODFSMounted(IsODFSMountedCallback callback) override;
  void SignInToOneDrive(SignInToOneDriveCallback callback) override;
  void RespondWithUserActionAndClose(mojom::UserAction action) override;
  void RespondWithLocalTaskAndClose(int task_position) override;
  void SetOfficeAsDefaultHandler() override;
  void GetAlwaysMoveOfficeFilesToDrive(
      GetAlwaysMoveOfficeFilesToDriveCallback callback) override;
  void SetAlwaysMoveOfficeFilesToDrive(bool always_move) override;
  void GetAlwaysMoveOfficeFilesToOneDrive(
      GetAlwaysMoveOfficeFilesToOneDriveCallback callback) override;
  void SetAlwaysMoveOfficeFilesToOneDrive(bool always_move) override;
  void GetOfficeMoveConfirmationShownForDrive(
      GetOfficeMoveConfirmationShownForDriveCallback callback) override;
  void GetOfficeMoveConfirmationShownForOneDrive(
      GetOfficeMoveConfirmationShownForOneDriveCallback callback) override;
  void RecordCancel(mojom::MetricsRecordedSetupPage page) override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebUI> web_ui_;
  mojom::DialogArgsPtr dialog_args_;
  bool odfs_mount_called_ = false;

  mojo::Receiver<PageHandler> receiver_;
  RespondWithUserActionAndCloseCallback user_action_callback_;
  RespondWithLocalTaskAndCloseCallback local_task_callback_;

  base::WeakPtrFactory<CloudUploadPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_PAGE_HANDLER_H_
