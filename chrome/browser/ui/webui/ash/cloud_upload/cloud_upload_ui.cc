// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"

#include "ash/webui/common/trusted_types_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/cloud_upload_resources.h"
#include "chrome/grit/cloud_upload_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash::cloud_upload {

bool CloudUploadUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return chromeos::IsEligibleAndEnabledUploadOfficeToCloud(
      Profile::FromBrowserContext(browser_context));
}

CloudUploadUI::CloudUploadUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUICloudUploadHost);

  static constexpr webui::LocalizedString kStrings[] = {
      // Dialog buttons.
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
      {"done", IDS_DONE},
      {"open", IDS_OFFICE_FILE_HANDLER_OPEN_BUTTON},
      {"install", IDS_INSTALL},
      {"installing", IDS_OFFICE_INSTALL_PWA_INSTALLING_BUTTON},
      {"installed", IDS_OFFICE_INSTALL_PWA_INSTALLED_BUTTON},
      {"cancelSetup", IDS_OFFICE_CANCEL_SETUP_CANCEL_BUTTON},
      {"continueSetup", IDS_OFFICE_CANCEL_SETUP_CONTINUE_BUTTON},
      {"animationPlayText", IDS_OOBE_PLAY_ANIMATION_MESSAGE},
      {"animationPauseText", IDS_OOBE_PAUSE_ANIMATION_MESSAGE},
      {"moveAndOpen", IDS_OFFICE_MOVE_CONFIRMATION_MOVE_BUTTON},
      {"copyAndOpen", IDS_OFFICE_MOVE_CONFIRMATION_COPY_BUTTON},
      // Connect To OneDrive dialog.
      {"connectToOneDriveTitle", IDS_CONNECT_TO_ONEDRIVE_TITLE},
      {"connectToOneDriveSignInFlowBodyText",
       IDS_CONNECT_TO_ONEDRIVE_SIGNIN_FLOW_BODY_TEXT},
      {"connectToOneDriveBodyText", IDS_CONNECT_TO_ONEDRIVE_BODY_TEXT},
      {"cantConnectOneDrive", IDS_CANT_CONNECT_ONEDRIVE},
      {"connectOneDrive", IDS_CONNECT_ONEDRIVE},
      {"oneDriveConnectedTitle", IDS_ONEDRIVE_CONNECTED_TITLE},
      {"oneDriveConnectedBodyText", IDS_ONEDRIVE_CONNECTED_BODY_TEXT},
      // File Handler selection dialog.
      {"fileHandlerTitle", IDS_OFFICE_FILE_HANDLER_TITLE},
      {"word", IDS_OFFICE_FILE_HANDLER_FILE_TYPE_WORD},
      {"excel", IDS_OFFICE_FILE_HANDLER_FILE_TYPE_EXCEL},
      {"powerPoint", IDS_OFFICE_FILE_HANDLER_FILE_TYPE_POWERPOINT},
      {"googleDocs", IDS_OFFICE_FILE_HANDLER_APP_GOOGLE_DOCS},
      {"googleSheets", IDS_OFFICE_FILE_HANDLER_APP_GOOGLE_SHEETS},
      {"googleSlides", IDS_OFFICE_FILE_HANDLER_APP_GOOGLE_SLIDES},
      {"microsoft365", IDS_OFFICE_FILE_HANDLER_APP_MICROSOFT},
      {"otherApps", IDS_OFFICE_FILE_HANDLER_APP_OTHERS},
      {"googleDriveStorage", IDS_OFFICE_FILE_HANDLER_STORAGE_GOOGLE},
      {"oneDriveStorage", IDS_OFFICE_FILE_HANDLER_STORAGE_MICROSOFT},
      // Install PWA dialog.
      {"installPWATitle", IDS_OFFICE_INSTALL_PWA_TITLE},
      {"installPWABodyText", IDS_OFFICE_INSTALL_PWA_BODY_TEXT},
      // Cancel setup dialog.
      {"cancelSetupTitle", IDS_OFFICE_CANCEL_SETUP_TITLE},
      {"cancelSetupBodyText", IDS_OFFICE_CANCEL_SETUP_BODY_TEXT},
      // OneDrive setup complete dialog.
      {"oneDriveSetupCompleteTitle", IDS_OFFICE_ONEDRIVE_SETUP_COMPLETE_TITLE},
      {"oneDriveSetupCompleteBodyText",
       IDS_OFFICE_ONEDRIVE_SETUP_COMPLETE_BODY_TEXT},
      // Welcome dialog.
      {"welcomeBodyText", IDS_OFFICE_WELCOME_BODY_TEXT},
      {"welcomeGetStarted", IDS_OFFICE_WELCOME_GET_STARTED},
      {"welcomeInstallOdfs", IDS_OFFICE_WELCOME_CONNECT_ONEDRIVE},
      {"welcomeInstallOfficeWebApp", IDS_OFFICE_WELCOME_INSTALL_MICROSOFT365},
      {"welcomeMoveFiles", IDS_OFFICE_WELCOME_FILES_WILL_MOVE},
      {"welcomeSetUp", IDS_OFFICE_WELCOME_SET_UP},
      {"welcomeTitle", IDS_OFFICE_WELCOME_TITLE},
      // Copy/Move confirmation dialog.
      {"moveConfirmationMoveTitle", IDS_OFFICE_MOVE_CONFIRMATION_MOVE_TITLE},
      {"moveConfirmationMoveTitlePlural",
       IDS_OFFICE_MOVE_CONFIRMATION_MOVE_TITLE_PLURAL},
      {"moveConfirmationCopyTitle", IDS_OFFICE_MOVE_CONFIRMATION_COPY_TITLE},
      {"moveConfirmationCopyTitlePlural",
       IDS_OFFICE_MOVE_CONFIRMATION_COPY_TITLE_PLURAL},
      {"moveConfirmationOneDriveBodyText",
       IDS_OFFICE_MOVE_CONFIRMATION_ONEDRIVE_BODY_TEXT},
      {"moveConfirmationGoogleDriveBodyText",
       IDS_OFFICE_MOVE_CONFIRMATION_GOOGLE_DRIVE_BODY_TEXT},
      {"moveConfirmationAlwaysMove",
       IDS_OFFICE_MOVE_CONFIRMATION_ALWAYS_MOVE_CHECKBOX},
      {"oneDrive", IDS_OFFICE_CLOUD_PROVIDER_ONEDRIVE},
      {"googleDrive", IDS_OFFICE_CLOUD_PROVIDER_GOOGLE_DRIVE},
  };
  source->AddLocalizedStrings(kStrings);
  webui::SetupWebUIDataSource(
      source, base::make_span(kCloudUploadResources, kCloudUploadResourcesSize),
      IDR_CLOUD_UPLOAD_MAIN_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");
  ash::EnableTrustedTypesCSP(source);
}

CloudUploadUI::~CloudUploadUI() = default;

void CloudUploadUI::SetDialogArgs(mojom::DialogArgsPtr args) {
  dialog_args_ = std::move(args);
}

void CloudUploadUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver) {
  if (factory_receiver_.is_bound()) {
    factory_receiver_.reset();
  }
  factory_receiver_.Bind(std::move(pending_receiver));
}

void CloudUploadUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void CloudUploadUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<CloudUploadPageHandler>(
      web_ui(), Profile::FromWebUI(web_ui()), std::move(dialog_args_),
      std::move(receiver),
      // base::Unretained() because |page_handler_| will not out-live |this|.
      base::BindOnce(&CloudUploadUI::RespondWithUserActionAndCloseDialog,
                     base::Unretained(this)),
      base::BindOnce(&CloudUploadUI::RespondWithLocalTaskAndCloseDialog,
                     base::Unretained(this)));
}

void CloudUploadUI::RespondWithUserActionAndCloseDialog(
    mojom::UserAction action) {
  base::Value::List args;
  switch (action) {
    case mojom::UserAction::kCancel:
      args.Append(kUserActionCancel);
      break;
    case mojom::UserAction::kCancelGoogleDrive:
      args.Append(kUserActionCancelGoogleDrive);
      break;
    case mojom::UserAction::kCancelOneDrive:
      args.Append(kUserActionCancelOneDrive);
      break;
    case mojom::UserAction::kSetUpOneDrive:
      args.Append(kUserActionSetUpOneDrive);
      break;
    case mojom::UserAction::kUploadToGoogleDrive:
      args.Append(kUserActionUploadToGoogleDrive);
      break;
    case mojom::UserAction::kUploadToOneDrive:
      args.Append(kUserActionUploadToOneDrive);
      break;
    case mojom::UserAction::kConfirmOrUploadToGoogleDrive:
      args.Append(kUserActionConfirmOrUploadToGoogleDrive);
      break;
    case mojom::UserAction::kConfirmOrUploadToOneDrive:
      args.Append(kUserActionConfirmOrUploadToOneDrive);
      break;
  }
  ui::MojoWebDialogUI::CloseDialog(args);
}

void CloudUploadUI::RespondWithLocalTaskAndCloseDialog(int task_position) {
  base::Value::List args;
  args.Append(base::NumberToString(task_position));
  ui::MojoWebDialogUI::CloseDialog(args);
}

WEB_UI_CONTROLLER_TYPE_IMPL(CloudUploadUI)

}  // namespace ash::cloud_upload
