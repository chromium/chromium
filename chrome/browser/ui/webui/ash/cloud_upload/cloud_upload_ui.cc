// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/cloud_upload_resources.h"
#include "chrome/grit/cloud_upload_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::cloud_upload {

bool CloudUploadUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsUploadOfficeToCloudEnabled();
}

CloudUploadUI::CloudUploadUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUICloudUploadHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
  };
  source->AddLocalizedStrings(kStrings);
  webui::SetupWebUIDataSource(
      source, base::make_span(kCloudUploadResources, kCloudUploadResourcesSize),
      IDR_CLOUD_UPLOAD_MAIN_HTML);
  source->DisableTrustedTypesCSP();
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
    case mojom::UserAction::kSetUpGoogleDrive:
      args.Append(kUserActionSetUpGoogleDrive);
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
