// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/connectivity_services.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/internet_detail_dialog_resources.h"
#include "chrome/grit/internet_detail_dialog_resources_map.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {

namespace {

// Matches the width of the Settings content.
constexpr int kInternetDetailDialogWidth = 640;

int s_internet_detail_dialog_count = 0;

void AddInternetStrings(content::WebUIDataSource* html_source) {
  // Add default strings first.
  ui::network_element::AddLocalizedStrings(html_source);
  ui::network_element::AddOncLocalizedStrings(html_source);
  ui::network_element::AddDetailsLocalizedStrings(html_source);
  // Add additional strings and overrides needed by the dialog.
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"networkButtonDisconnect", IDS_SETTINGS_INTERNET_BUTTON_DISCONNECT},
      {"networkButtonForget", IDS_SETTINGS_INTERNET_BUTTON_FORGET},
      {"networkButtonSignin", IDS_SETTINGS_INTERNET_BUTTON_SIGNIN},
      {"networkIPAddress", IDS_SETTINGS_INTERNET_NETWORK_IP_ADDRESS},
      {"networkSectionNetwork", IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK},
      {"networkSectionProxy", IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY},
      {"networkIPConfigAuto", IDS_SETTINGS_INTERNET_NETWORK_IP_CONFIG_AUTO},
      {"save", IDS_SAVE},
      // Override for ui::network_element::AddDetailsLocalizedStrings
      {"networkProxyConnectionType",
       IDS_SETTINGS_INTERNET_NETWORK_PROXY_CONNECTION_TYPE_DIALOG},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
}

std::string GetNetworkName8(const NetworkState& network) {
  return network.Matches(NetworkTypePattern::Ethernet())
             ? l10n_util::GetStringUTF8(IDS_NETWORK_TYPE_ETHERNET)
             : network.name();
}

class PortalNetworkMessageHandler : public content::WebUIMessageHandler {
 public:
  PortalNetworkMessageHandler() = default;
  ~PortalNetworkMessageHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "showPortalSignin",
        base::BindRepeating(&PortalNetworkMessageHandler::ShowPortalSignin,
                            base::Unretained(this)));
  }

 private:
  void ShowPortalSignin(const base::Value::List& args) {
    if (args.size() < 1 || !args[0].is_string()) {
      NOTREACHED_IN_MIGRATION() << "Invalid args for: ShowPortalSignin";
      return;
    }
    const std::string& guid = args[0].GetString();
    NetworkConnect::Get()->ShowPortalSignin(guid,
                                            NetworkConnect::Source::kSettings);
  }
};

}  // namespace

// static
bool InternetDetailDialog::IsShown() {
  return s_internet_detail_dialog_count > 0;
}

// static
void InternetDetailDialog::ShowDialog(const std::string& network_id,
                                      gfx::NativeWindow parent) {
  auto* network_state_handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network;
  if (!network_id.empty())
    network = network_state_handler->GetNetworkStateFromGuid(network_id);
  else
    network = network_state_handler->DefaultNetwork();
  if (!network) {
    LOG(ERROR) << "Network not found: " << network_id;
    return;
  }
  auto* instance = SystemWebDialogDelegate::FindInstance(network->guid());
  if (instance) {
    instance->Focus();
    return;
  }

  InternetDetailDialog* dialog = new InternetDetailDialog(*network);
  dialog->ShowSystemDialog(parent);
}

InternetDetailDialog::InternetDetailDialog(const NetworkState& network)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIInternetDetailDialogURL),
                              /* title= */ std::u16string()),
      network_id_(network.guid()),
      network_type_(network_util::TranslateShillTypeToONC(network.type())),
      network_name_(GetNetworkName8(network)) {
  ++s_internet_detail_dialog_count;
}

InternetDetailDialog::~InternetDetailDialog() {
  --s_internet_detail_dialog_count;
}

std::string InternetDetailDialog::Id() {
  return network_id_;
}

void InternetDetailDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kInternetDetailDialogWidth,
                SystemWebDialogDelegate::kDialogHeight);
}

std::string InternetDetailDialog::GetDialogArgs() const {
  base::Value::Dict args;
  args.Set("type", network_type_);
  args.Set("guid", network_id_);
  args.Set("name", network_name_);
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

// InternetDetailDialogUI

InternetDetailDialogUI::InternetDetailDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PortalNetworkMessageHandler>());

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIInternetDetailDialogHost);
  source->AddBoolean("showTechnologyBadge",
                     !features::IsSeparateNetworkIconsEnabled());
  source->AddBoolean("apnRevamp", features::IsApnRevampEnabled());
  cellular_setup::AddNonStringLoadTimeData(source);
  AddInternetStrings(source);
  source->AddLocalizedString("title", IDS_SETTINGS_INTERNET_DETAIL);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kInternetDetailDialogResources,
                      kInternetDetailDialogResourcesSize),
      IDR_INTERNET_DETAIL_DIALOG_INTERNET_DETAIL_DIALOG_CONTAINER_HTML);
  source->DisableTrustedTypesCSP();
}

InternetDetailDialogUI::~InternetDetailDialogUI() {}

void InternetDetailDialogUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
}

void InternetDetailDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void InternetDetailDialogUI::BindInterface(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        receiver) {
  ash::GetPasspointService(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(InternetDetailDialogUI)

}  // namespace ash
