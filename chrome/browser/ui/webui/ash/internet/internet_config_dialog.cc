// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/internet_config_dialog_resources.h"
#include "chrome/grit/internet_config_dialog_resources_map.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Dialog height for configured networks that only require a passphrase.
// This height includes room for a 'connecting' or error message.
constexpr int kDialogHeightPasswordOnly = 365;

void AddInternetStrings(content::WebUIDataSource* html_source) {
  // Add default strings first.
  ui::network_element::AddLocalizedStrings(html_source);
  ui::network_element::AddOncLocalizedStrings(html_source);
  ui::network_element::AddConfigLocalizedStrings(html_source);
  ui::network_element::AddErrorLocalizedStrings(html_source);
  // Add additional strings and overrides needed by the dialog.
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"internetJoinType", IDS_SETTINGS_INTERNET_JOIN_TYPE},
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"cancel", IDS_CANCEL},
      {"close", IDS_CANCEL},
      {"save", IDS_SAVE},
  };
  for (const auto& entry : localized_strings) {
    html_source->AddLocalizedString(entry.name, entry.id);
  }
}

std::string GetId(const std::string& network_type,
                  const std::string& network_id) {
  std::string result = chrome::kChromeUIInternetConfigDialogURL + network_type;
  if (!network_id.empty()) {
    result += ".";
    result += network_id;
  }
  return result;
}

}  // namespace

// static
void InternetConfigDialog::ShowDialogForNetworkId(const std::string& network_id,
                                                  gfx::NativeWindow parent) {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          network_id);
  if (!network_state) {
    LOG(ERROR) << "Network not found: " << network_id;
    return;
  }
  std::string network_type =
      network_util::TranslateShillTypeToONC(network_state->type());
  std::string id = GetId(network_type, network_id);
  auto* instance = SystemWebDialogDelegate::FindInstance(id);
  if (instance) {
    instance->Focus();
    return;
  }

  InternetConfigDialog* dialog = new InternetConfigDialog(
      id, network_type, network_id, /*prefilled_wifi_config=*/std::nullopt);
  dialog->ShowSystemDialog(parent);
}

// static
void InternetConfigDialog::ShowDialogForNetworkType(
    const std::string& network_type,
    gfx::NativeWindow parent) {
  std::string id = GetId(network_type, std::string());
  auto* instance = SystemWebDialogDelegate::FindInstance(id);
  if (instance) {
    instance->Focus();
    return;
  }

  InternetConfigDialog* dialog =
      new InternetConfigDialog(id, network_type, /*network_id=*/std::string(),
                               /*prefilled_wifi_config=*/std::nullopt);
  dialog->ShowSystemDialog(parent);
}

// static
void InternetConfigDialog::ShowDialogForNetworkWithWifiConfig(
    mojo::StructPtr<chromeos::network_config::mojom::WiFiConfigProperties>
        wifi_config,
    gfx::NativeWindow parent) {
  const std::string network_type = onc::network_type::kWiFi;
  const std::string id = GetId(network_type, std::string());
  auto* instance = SystemWebDialogDelegate::FindInstance(id);
  if (instance) {
    LOG(ERROR)
        << "Dialog is already on. The provided Wi-Fi config will be dropped";
    instance->Focus();
    return;
  }
  InternetConfigDialog* dialog = new InternetConfigDialog(
      id, network_type, /*network_id=*/std::string(),
      /*prefilled_wifi_config=*/std::move(wifi_config));
  dialog->ShowSystemDialog(parent);
}

InternetConfigDialog::InternetConfigDialog(
    const std::string& dialog_id,
    const std::string& network_type,
    const std::string& network_id,
    std::optional<
        mojo::StructPtr<chromeos::network_config::mojom::WiFiConfigProperties>>
        prefilled_wifi_config)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIInternetConfigDialogURL),
                              std::u16string() /* title */),
      dialog_id_(dialog_id),
      network_type_(network_type),
      network_id_(network_id),
      prefilled_wifi_config_(std::move(prefilled_wifi_config)) {}

InternetConfigDialog::~InternetConfigDialog() = default;

std::string InternetConfigDialog::Id() {
  return dialog_id_;
}

void InternetConfigDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->shadow_elevation = wm::kShadowElevationActiveWindow;
}

void InternetConfigDialog::GetDialogSize(gfx::Size* size) const {
  const NetworkState* network =
      network_id_.empty() ? nullptr
                          : NetworkHandler::Get()
                                ->network_state_handler()
                                ->GetNetworkStateFromGuid(network_id_);
  int height = network && network->SecurityRequiresPassphraseOnly()
                   ? kDialogHeightPasswordOnly
                   : InternetConfigDialog::kDialogHeight;
  size->SetSize(InternetConfigDialog::kDialogWidth, height);
}

std::string InternetConfigDialog::GetDialogArgs() const {
  base::Value::Dict args;
  args.Set("type", network_type_);
  args.Set("guid", network_id_);

  if (prefilled_wifi_config_.has_value()) {
    base::Value::Dict prefilled_properties =
        chromeos::network_config::WiFiConfigPropertiesToMojoJsValue(
            *prefilled_wifi_config_);
    args.Set("prefilledProperties", std::move(prefilled_properties));
  }
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

// InternetConfigDialogUI

InternetConfigDialogUI::InternetConfigDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIInternetConfigDialogHost);

  AddInternetStrings(source);
  source->AddLocalizedString("title", IDS_SETTINGS_INTERNET_CONFIG);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kInternetConfigDialogResources,
                      kInternetConfigDialogResourcesSize),
      IDR_INTERNET_CONFIG_DIALOG_INTERNET_CONFIG_DIALOG_CONTAINER_HTML);
  // Enabling trusted types via trusted_types_util must be done after
  // webui::SetupWebUIDataSource to override the trusted type CSP with correct
  // policies for JS WebUIs.
  ash::EnableTrustedTypesCSP(source);
}

InternetConfigDialogUI::~InternetConfigDialogUI() {}

void InternetConfigDialogUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
}

void InternetConfigDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_change_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(InternetConfigDialogUI)

}  // namespace ash
