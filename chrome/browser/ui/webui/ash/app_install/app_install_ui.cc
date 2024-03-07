// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_ui.h"

#include "ash/webui/common/trusted_types_util.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/app_install_resources.h"
#include "chrome/grit/app_install_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash::app_install {

AppInstallDialogUI::AppInstallDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIAppInstallDialogHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"cancel", IDS_CANCEL},
      {"install", IDS_INSTALL},
      {"installing", IDS_OFFICE_INSTALL_PWA_INSTALLING_BUTTON},
      {"openApp", IDS_OPEN_APP},
      {"developerInformation", IDS_DEVELOPER_INFORMATION},
      {"installingApp", IDS_INSTALLING_APP},
      {"appInstalled", IDS_APP_INSTALLED},
  };

  source->AddLocalizedStrings(kStrings);
  source->AddString("installAppToDevice",
                    l10n_util::GetStringFUTF8(IDS_INSTALL_DIALOG_TITLE,
                                              ui::GetChromeOSDeviceName()));

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kAppInstallResources, kAppInstallResourcesSize),
      IDR_APP_INSTALL_MAIN_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));

  ash::EnableTrustedTypesCSP(source);
}

AppInstallDialogUI::~AppInstallDialogUI() = default;

void AppInstallDialogUI::SetDialogArgs(mojom::DialogArgsPtr args) {
  dialog_args_ = std::move(args);
}

void AppInstallDialogUI::SetExpectedAppId(std::string expected_app_id) {
  expected_app_id_ = expected_app_id;
}

void AppInstallDialogUI::SetDialogCallback(
    base::OnceCallback<void(bool accepted)> dialog_accepted_callback) {
  dialog_accepted_callback_ = std::move(dialog_accepted_callback);
}

void AppInstallDialogUI::SetInstallComplete(const std::string* app_id) {
  if (!page_handler_) {
    return;
  }
  page_handler_->OnInstallComplete(app_id);
}

void AppInstallDialogUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver) {
  if (factory_receiver_.is_bound()) {
    factory_receiver_.reset();
  }
  factory_receiver_.Bind(std::move(pending_receiver));
}

void AppInstallDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void AppInstallDialogUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<AppInstallPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(dialog_args_),
      std::move(expected_app_id_), std::move(dialog_accepted_callback_),
      std::move(receiver),
      base::BindOnce(&AppInstallDialogUI::CloseDialog, base::Unretained(this)));
}

void AppInstallDialogUI::CloseDialog() {
  ui::MojoWebDialogUI::CloseDialog(base::Value::List());
}

WEB_UI_CONTROLLER_TYPE_IMPL(AppInstallDialogUI)

bool AppInstallDialogUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return (base::FeatureList::IsEnabled(
              chromeos::features::kCrosWebAppInstallDialog) ||
          base::FeatureList::IsEnabled(
              chromeos::features::kCrosOmniboxInstallDialog));
}

AppInstallDialogUIConfig::AppInstallDialogUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIAppInstallDialogHost) {}

}  // namespace ash::app_install
