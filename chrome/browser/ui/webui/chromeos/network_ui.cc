// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/esim_manager.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/net/network_health/network_health_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_dialog_launcher.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/browser/ui/webui/chromeos/network_logs_message_handler.h"
#include "chrome/browser/ui/webui/chromeos/onc_import_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/network_ui_resources.h"
#include "chrome/grit/network_ui_resources_map.h"
#include "chromeos/components/network_ui/network_diagnostics_resource_provider.h"
#include "chromeos/components/network_ui/network_health_resource_provider.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr char kAddNetwork[] = "addNetwork";
constexpr char kGetNetworkProperties[] = "getShillNetworkProperties";
constexpr char kGetDeviceProperties[] = "getShillDeviceProperties";
constexpr char kGetEthernetEAP[] = "getShillEthernetEAP";
constexpr char kOpenCellularActivationUi[] = "openCellularActivationUi";
constexpr char kShowNetworkDetails[] = "showNetworkDetails";
constexpr char kShowNetworkConfig[] = "showNetworkConfig";
constexpr char kShowAddNewWifiNetworkDialog[] = "showAddNewWifi";
constexpr char kGetHostname[] = "getHostname";
constexpr char kSetHostname[] = "setHostname";

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network)
    return false;
  *service_path = network->path();
  return true;
}

void SetDeviceProperties(base::Value* dictionary) {
  const std::string* device = dictionary->FindStringKey(shill::kDeviceProperty);
  if (!device)
    return;
  const DeviceState* device_state =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(*device);
  if (!device_state)
    return;

  base::Value device_dictionary(device_state->properties().Clone());
  if (!device_state->ip_configs().empty()) {
    // Convert IPConfig dictionary to a ListValue.
    base::Value ip_configs(base::Value::Type::LIST);
    for (auto iter : device_state->ip_configs().DictItems()) {
      ip_configs.Append(iter.second.Clone());
    }
    device_dictionary.SetKey(shill::kIPConfigsProperty, std::move(ip_configs));
  }
  if (!device_dictionary.DictEmpty())
    dictionary->SetKey(shill::kDeviceProperty, std::move(device_dictionary));
}

class NetworkConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkConfigMessageHandler() {}
  ~NetworkConfigMessageHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        kAddNetwork,
        base::BindRepeating(&NetworkConfigMessageHandler::AddNetwork,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetNetworkProperties,
        base::BindRepeating(
            &NetworkConfigMessageHandler::GetShillNetworkProperties,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetDeviceProperties,
        base::BindRepeating(
            &NetworkConfigMessageHandler::GetShillDeviceProperties,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetEthernetEAP,
        base::BindRepeating(&NetworkConfigMessageHandler::GetShillEthernetEAP,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kOpenCellularActivationUi,
        base::BindRepeating(
            &NetworkConfigMessageHandler::OpenCellularActivationUi,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowNetworkDetails,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkDetails,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowNetworkConfig,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkConfig,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowAddNewWifiNetworkDialog,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowAddNewWifi,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetHostname,
        base::BindRepeating(&NetworkConfigMessageHandler::GetHostname,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kSetHostname,
        base::BindRepeating(&NetworkConfigMessageHandler::SetHostname,
                            base::Unretained(this)));
  }

 private:
  void Respond(const std::string& callback_id, const base::Value& response) {
    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), response);
  }

  void GetShillNetworkProperties(const base::ListValue* arg_list) {
    CHECK_EQ(2u, arg_list->GetSize());
    std::string callback_id, guid;
    CHECK(arg_list->GetString(0, &callback_id));
    CHECK(arg_list->GetString(1, &guid));

    std::string service_path;
    if (!GetServicePathFromGuid(guid, &service_path)) {
      RunErrorCallback(callback_id, guid, kGetNetworkProperties,
                       "Error.InvalidNetworkGuid");
      return;
    }
    NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
        service_path,
        base::BindOnce(
            &NetworkConfigMessageHandler::OnGetShillNetworkProperties,
            weak_ptr_factory_.GetWeakPtr(), callback_id, guid));
  }

  void OnGetShillNetworkProperties(const std::string& callback_id,
                                   const std::string& guid,
                                   const std::string& service_path,
                                   base::Optional<base::Value> result) {
    if (!result) {
      RunErrorCallback(callback_id, guid, kGetNetworkProperties, "Error.DBus");
      return;
    }
    // Set the 'service_path' property for debugging.
    result->SetKey("service_path", base::Value(service_path));
    // Set the device properties for debugging.
    SetDeviceProperties(&result.value());
    base::ListValue return_arg_list;
    return_arg_list.Append(std::move(*result));
    Respond(callback_id, return_arg_list);
  }

  void GetShillDeviceProperties(const base::ListValue* arg_list) {
    CHECK_EQ(2u, arg_list->GetSize());
    std::string callback_id, type;
    CHECK(arg_list->GetString(0, &callback_id));
    CHECK(arg_list->GetString(1, &type));

    const DeviceState* device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
            onc::NetworkTypePatternFromOncType(type));
    if (!device) {
      RunErrorCallback(callback_id, type, kGetDeviceProperties,
                       "Error.InvalidDeviceType");
      return;
    }
    NetworkHandler::Get()->network_device_handler()->GetDeviceProperties(
        device->path(),
        base::BindOnce(&NetworkConfigMessageHandler::OnGetShillDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr(), callback_id, type));
  }

  void GetShillEthernetEAP(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string callback_id;
    CHECK(arg_list->GetString(0, &callback_id));

    NetworkStateHandler::NetworkStateList list;
    NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
        NetworkTypePattern::Primitive(shill::kTypeEthernetEap),
        true /* configured_only */, false /* visible_only */, 1 /* limit */,
        &list);

    if (list.empty()) {
      Respond(callback_id, base::Value(base::Value::Type::LIST));
      return;
    }
    const NetworkState* eap = list.front();
    base::Value properties(base::Value::Type::DICTIONARY);
    properties.SetStringKey("guid", eap->guid());
    properties.SetStringKey("name", eap->name());
    properties.SetStringKey("type", eap->type());
    base::Value response(base::Value::Type::LIST);
    response.Append(std::move(properties));
    Respond(callback_id, response);
  }

  void OpenCellularActivationUi(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string callback_id;
    CHECK(arg_list->GetString(0, &callback_id));

    const NetworkState* cellular_network =
        NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
            NetworkTypePattern::Cellular());
    if (cellular_network)
      cellular_setup::OpenCellularSetupDialog(cellular_network->guid());

    base::Value response(base::Value::Type::LIST);
    response.Append(base::Value(cellular_network != nullptr));
    Respond(callback_id, response);
  }

  void ShowNetworkDetails(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string guid;
    CHECK(arg_list->GetString(0, &guid));

    InternetDetailDialog::ShowDialog(guid);
  }

  void ShowNetworkConfig(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string guid;
    CHECK(arg_list->GetString(0, &guid));

    InternetConfigDialog::ShowDialogForNetworkId(guid);
  }

  void ShowAddNewWifi(const base::ListValue* arg_list) {
    InternetConfigDialog::ShowDialogForNetworkType(::onc::network_type::kWiFi);
  }

  void OnGetShillDeviceProperties(const std::string& callback_id,
                                  const std::string& type,
                                  const std::string& device_path,
                                  base::Optional<base::Value> result) {
    if (!result) {
      RunErrorCallback(callback_id, type, kGetDeviceProperties,
                       "GetDeviceProperties failed");
      return;
    }

    // Set the 'device_path' property for debugging.
    result->SetKey("device_path", base::Value(device_path));

    base::ListValue return_arg_list;
    return_arg_list.Append(std::move(*result));
    Respond(callback_id, return_arg_list);
  }

  void GetHostname(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string callback_id;
    CHECK(arg_list->GetString(0, &callback_id));
    std::string hostname =
        NetworkHandler::Get()->network_state_handler()->hostname();
    Respond(callback_id, base::Value(hostname));
  }

  void SetHostname(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string hostname;
    CHECK(arg_list->GetString(0, &hostname));
    NET_LOG(USER) << "SET HOSTNAME: " << hostname;
    NetworkHandler::Get()->network_state_handler()->SetHostname(hostname);
  }

  void ErrorCallback(const std::string& callback_id,
                     const std::string& guid_or_type,
                     const std::string& function_name,
                     const std::string& error_name,
                     std::unique_ptr<base::DictionaryValue> /* error_data */) {
    RunErrorCallback(callback_id, guid_or_type, function_name, error_name);
  }

  void RunErrorCallback(const std::string& callback_id,
                        const std::string& guid_or_type,
                        const std::string& function_name,
                        const std::string& error_name) {
    NET_LOG(ERROR) << "Shill Error: " << error_name << " id=" << guid_or_type;
    base::ListValue return_arg_list;
    base::Value dictionary(base::Value::Type::DICTIONARY);
    std::string key = function_name == kGetDeviceProperties
                          ? shill::kTypeProperty
                          : shill::kGuidProperty;
    dictionary.SetKey(key, base::Value(guid_or_type));
    dictionary.SetKey("ShillError", base::Value(error_name));
    return_arg_list.Append(std::move(dictionary));
    Respond(callback_id, return_arg_list);
  }

  void AddNetwork(const base::ListValue* args) {
    std::string onc_type;
    args->GetString(0, &onc_type);
    InternetConfigDialog::ShowDialogForNetworkType(onc_type);
  }

  base::WeakPtrFactory<NetworkConfigMessageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigMessageHandler);
};

class NetworkDiagnosticsMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkDiagnosticsMessageHandler() = default;
  ~NetworkDiagnosticsMessageHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "OpenFeedbackDialog",
        base::BindRepeating(
            &NetworkDiagnosticsMessageHandler::OpenFeedbackDialog,
            base::Unretained(this)));
  }

 private:
  void OpenFeedbackDialog(const base::ListValue* value) {
    const std::string result = value->GetList()[0].GetString();
    chrome::ShowFeedbackPage(nullptr, chrome::kFeedbackSourceNetworkHealthPage,
                             "" /*description_template*/,
                             "" /*description_template_placeholder*/,
                             "network-health", result);
  }
};

}  // namespace

// static
void NetworkUI::GetLocalizedStrings(base::DictionaryValue* localized_strings) {
  localized_strings->SetString("titleText",
                               l10n_util::GetStringUTF16(IDS_NETWORK_UI_TITLE));

  localized_strings->SetString(
      "generalTab", l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_GENERAL));
  localized_strings->SetString(
      "networkHealthTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_HEALTH));
  localized_strings->SetString(
      "networkLogsTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_LOGS));
  localized_strings->SetString(
      "networkStateTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_STATE));
  localized_strings->SetString(
      "networkSelectTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_SELECT));

  localized_strings->SetString(
      "autoRefreshText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_AUTO_REFRESH));
  localized_strings->SetString(
      "deviceLogLinkText", l10n_util::GetStringUTF16(IDS_DEVICE_LOG_LINK_TEXT));
  localized_strings->SetString(
      "networkRefreshText", l10n_util::GetStringUTF16(IDS_NETWORK_UI_REFRESH));
  localized_strings->SetString(
      "clickToExpandText", l10n_util::GetStringUTF16(IDS_NETWORK_UI_EXPAND));
  localized_strings->SetString(
      "propertyFormatText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_PROPERTY_FORMAT));

  localized_strings->SetString(
      "normalFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_NORMAL));
  localized_strings->SetString(
      "managedFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_MANAGED));
  localized_strings->SetString(
      "stateFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_STATE));
  localized_strings->SetString(
      "shillFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_SHILL));

  localized_strings->SetString(
      "dhcpHostnameLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_DHCP_HOSTNAME));
  localized_strings->SetString(
      "globalPolicyLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_GLOBAL_POLICY));
  localized_strings->SetString(
      "networkListsLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LISTS));
  localized_strings->SetString(
      "networkHealthLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_HEALTH));
  localized_strings->SetString(
      "networkDiagnosticsLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_DIAGNOSTICS));
  localized_strings->SetString(
      "visibleNetworksLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_VISIBLE_NETWORKS));
  localized_strings->SetString(
      "favoriteNetworksLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FAVORITE_NETWORKS));
  localized_strings->SetString(
      "ethernetEapNetworkLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_ETHERNET_EAP));
  localized_strings->SetString(
      "devicesLabel", l10n_util::GetStringUTF16(IDS_NETWORK_UI_DEVICES));

  localized_strings->SetString(
      "cellularActivationLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NO_CELLULAR_ACTIVATION_LABEL));
  localized_strings->SetString(
      "cellularActivationButtonText",
      l10n_util::GetStringUTF16(
          IDS_NETWORK_UI_OPEN_CELLULAR_ACTIVATION_BUTTON_TEXT));
  localized_strings->SetString(
      "noCellularErrorText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NO_CELLULAR_ERROR_TEXT));

  localized_strings->SetString(
      "addNewWifiLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_ADD_NEW_WIFI_LABEL));
  localized_strings->SetString(
      "addNewWifiButtonText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_ADD_NEW_WIFI_BUTTON_TEXT));

  localized_strings->SetString(
      "importOncButtonText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_IMPORT_ONC_BUTTON_TEXT));

  localized_strings->SetString(
      "addWiFiListItemName",
      l10n_util::GetStringUTF16(IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME));

  // Network logs
  localized_strings->SetString(
      "networkLogsDescription",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DESCRIPTION));
  localized_strings->SetString(
      "networkLogsSystemLogs",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_SYSTEM_LOGS));
  localized_strings->SetString(
      "networkLogsFilterPii",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_FILTER_PII));
  localized_strings->SetString(
      "networkLogsPolicies",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_POLICIES));
  localized_strings->SetString(
      "networkLogsDebugLogs",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUG_LOGS));
  localized_strings->SetString(
      "networkLogsChromeLogs",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_CHROME_LOGS));
  localized_strings->SetString(
      "networkLogsStoreButton",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_STORE_BUTTON));
  localized_strings->SetString(
      "networkLogsStatus",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_STATUS));
  localized_strings->SetString(
      "networkLogsDebuggingTitle",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_TITLE));
  localized_strings->SetString(
      "networkLogsDebuggingDescription",
      l10n_util::GetStringUTF16(
          IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_DESCRIPTION));
  localized_strings->SetString(
      "networkLogsDebuggingNone",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_NONE));
  localized_strings->SetString(
      "networkLogsDebuggingUnknown",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_UNKNOWN));

  // Network Diagnostics
  localized_strings->SetString(
      "NetworkDiagnosticsRunAll",
      l10n_util::GetStringUTF16(IDS_NETWORK_DIAGNOSTICS_RUN_ALL));
  localized_strings->SetString(
      "NetworkDiagnosticsSendFeedback",
      l10n_util::GetStringUTF16(IDS_NETWORK_DIAGNOSTICS_SEND_FEEDBACK));
}

NetworkUI::NetworkUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  web_ui->AddMessageHandler(std::make_unique<NetworkConfigMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<OncImportMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<NetworkLogsMessageHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<NetworkDiagnosticsMessageHandler>());

  // Enable extension API calls in the WebUI.
  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUINetworkHost);

  html->DisableTrustedTypesCSP();

  html->AddLocalizedStrings(localized_strings);
  network_health::AddResources(html);
  network_diagnostics::AddResources(html);
  cellular_setup::AddLocalizedStrings(html);
  cellular_setup::AddNonStringLoadTimeData(html);
  network_element::AddLocalizedStrings(html);
  network_element::AddOncLocalizedStrings(html);
  html->UseStringsJs();

  webui::SetupWebUIDataSource(
      html, base::make_span(kNetworkUiResources, kNetworkUiResourcesSize),
      IDR_NETWORK_UI_NETWORK_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html);
}

NetworkUI::~NetworkUI() {}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<network_health::mojom::NetworkHealthService>
        receiver) {
  network_health::NetworkHealthService::GetInstance()->BindHealthReceiver(
      std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<
        network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver) {
  network_health::NetworkHealthService::GetInstance()->BindDiagnosticsReceiver(
      std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver) {
  ash::GetESimManager(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(NetworkUI)

}  // namespace chromeos
