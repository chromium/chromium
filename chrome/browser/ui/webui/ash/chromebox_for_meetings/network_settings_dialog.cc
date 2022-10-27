// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/chromebox_for_meetings/network_settings_dialog.h"

#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/ash/app_mode/certificate_manager_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/ash/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/cfm_network_settings_resources.h"
#include "chrome/grit/cfm_network_settings_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"

namespace ash::cfm {

namespace {

// Use Meet Devices settings dialog size.
constexpr int kCfmDialogWidth = 800;
constexpr int kCfmDialogHeight = 650;

bool g_cfm_network_settings_shown = false;

class NetworkSettingsMessageHandler : public content::WebUIMessageHandler {
 public:
  void RegisterMessages() override {
    web_ui()->RegisterHandlerCallback(
        "showNetworkConfig",
        base::BindRepeating(&NetworkSettingsMessageHandler::ShowNetworkConfig,
                            base::Unretained(this)));
    web_ui()->RegisterHandlerCallback(
        "showNetworkDetails",
        base::BindRepeating(&NetworkSettingsMessageHandler::ShowNetworkDetails,
                            base::Unretained(this)));
    web_ui()->RegisterHandlerCallback(
        "showAddWifi",
        base::BindRepeating(&NetworkSettingsMessageHandler::ShowAddWifi,
                            base::Unretained(this)));
    web_ui()->RegisterHandlerCallback(
        "showManageCerts",
        base::BindRepeating(&NetworkSettingsMessageHandler::ShowManageCerts,
                            base::Unretained(this)));
  }

  void ShowNetworkConfig(const std::string& guid) {
    InternetConfigDialog::ShowDialogForNetworkId(guid);
  }

  void ShowNetworkDetails(const std::string& guid) {
    InternetDetailDialog::ShowDialog(guid);
  }

  void ShowAddWifi() {
    InternetConfigDialog::ShowDialogForNetworkType(::onc::network_type::kWiFi);
  }

  void ShowManageCerts() {
    // Dialogs manage their own lifecycle and will delete themselves.
    CertificateManagerDialog* dialog = new CertificateManagerDialog(
        ProfileManager::GetActiveUserProfile(), nullptr);
    dialog->Show();
  }
};

}  // namespace

bool NetworkSettingsDialog::IsShown() {
  return g_cfm_network_settings_shown;
}

void NetworkSettingsDialog::ShowDialog() {
  if (!NetworkSettingsDialog::IsShown()) {
    // Dialogs handle their own lifecycle. Deleted in
    // SystemWebDialogDelegate::OnDialogClosed.
    NetworkSettingsDialog* dialog = new NetworkSettingsDialog();
    dialog->ShowSystemDialog();
  }
}

NetworkSettingsDialog::NetworkSettingsDialog()
    : SystemWebDialogDelegate(
          GURL(chrome::kCfmNetworkSettingsURL),
          l10n_util::GetStringUTF16(IDS_CFM_NETWORK_SETTINGS_TITLE)) {
  g_cfm_network_settings_shown = true;
}

void NetworkSettingsDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kCfmDialogWidth, kCfmDialogHeight);
}

void NetworkSettingsDialog::OnDialogClosed(const std::string& json_retval) {
  g_cfm_network_settings_shown = false;
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

NetworkSettingsDialogUi::NetworkSettingsDialogUi(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kCfmNetworkSettingsHost);

  source->AddLocalizedString("headTitle", IDS_CFM_NETWORK_SETTINGS_TITLE);
  source->AddLocalizedString("availableNetworks",
                             IDS_CFM_NETWORK_SETTINGS_AVAILABLE_NETWORKS);
  source->AddLocalizedString("addWiFiListItemName",
                             IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME);
  source->AddLocalizedString("proxySettingsListItemName",
                             IDS_NETWORK_PROXY_SETTINGS_LIST_ITEM_NAME);
  source->AddLocalizedString("manageCertsListItemName",
                             IDS_MANAGE_CERTIFICATES);
  ui::network_element::AddLocalizedStrings(source);
  cellular_setup::AddNonStringLoadTimeData(source);
  source->UseStringsJs();

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kCfmNetworkSettingsResources,
                      kCfmNetworkSettingsResourcesSize),
      IDR_CFM_NETWORK_SETTINGS_CFM_NETWORK_SETTINGS_CONTAINER_HTML);

  web_ui->AddMessageHandler(std::make_unique<NetworkSettingsMessageHandler>());

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

void NetworkSettingsDialogUi::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(NetworkSettingsDialogUi)

}  // namespace ash::cfm
