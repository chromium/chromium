// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/network_ui/network_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/connectivity_services.h"
#include "ash/public/cpp/esim_manager.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/network_ui/network_diagnostics_resource_provider.h"
#include "ash/webui/network_ui/network_health_resource_provider.h"
#include "ash/webui/network_ui/traffic_counters_resource_provider.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/net/network_health/network_health_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/webui/ash/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/network_ui/network_logs_message_handler.h"
#include "chrome/browser/ui/webui/ash/network_ui/onc_import_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/network_ui_resources.h"
#include "chrome/grit/network_ui_resources_map.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/ash/components/network/cellular_esim_uninstall_handler.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"

namespace ash {

namespace {

constexpr char kAddNetwork[] = "addNetwork";
constexpr char kDisableESimProfile[] = "disableActiveESimProfile";
constexpr char kGetNetworkProperties[] = "getShillNetworkProperties";
constexpr char kGetFirstWifiNetworkProperties[] =
    "getFirstWifiNetworkProperties";
constexpr char kGetDeviceProperties[] = "getShillDeviceProperties";
constexpr char kGetEthernetEAP[] = "getShillEthernetEAP";
constexpr char kOpenCellularActivationUi[] = "openCellularActivationUi";
constexpr char kResetESimCache[] = "resetESimCache";
constexpr char kResetEuicc[] = "resetEuicc";
constexpr char kResetApnMigrator[] = "resetApnMigrator";
constexpr char kShowNetworkDetails[] = "showNetworkDetails";
constexpr char kShowNetworkConfig[] = "showNetworkConfig";
constexpr char kShowAddNewWifiNetworkDialog[] = "showAddNewWifi";
constexpr char kGetHostname[] = "getHostname";
constexpr char kSetHostname[] = "setHostname";
constexpr char kGetTetheringCapabilities[] = "getTetheringCapabilities";
constexpr char kGetTetheringStatus[] = "getTetheringStatus";
constexpr char kGetTetheringConfig[] = "getTetheringConfig";
constexpr char kSetTetheringConfig[] = "setTetheringConfig";
constexpr char kCheckTetheringReadiness[] = "checkTetheringReadiness";
constexpr char kGetWifiDirectCapabilities[] = "getWifiDirectCapabilities";
constexpr char kGetWifiDirectOwnerInfo[] = "getWifiDirectOwnerInfo";
constexpr char kGetWifiDirectClientInfo[] = "getWifiDirectClientInfo";

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

void SetDeviceProperties(base::Value::Dict* dictionary) {
  DCHECK(dictionary);
  const std::string* device = dictionary->FindString(shill::kDeviceProperty);
  if (!device)
    return;
  const DeviceState* device_state =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(*device);
  if (!device_state)
    return;

  base::Value::Dict device_dictionary = device_state->properties().Clone();
  if (!device_state->ip_configs().empty()) {
    // Convert IPConfig dictionary to a ListValue.
    base::Value::List ip_configs;
    for (auto iter : device_state->ip_configs()) {
      ip_configs.Append(iter.second.Clone());
    }
    device_dictionary.Set(shill::kIPConfigsProperty, std::move(ip_configs));
  }
  if (!device_dictionary.empty())
    dictionary->Set(shill::kDeviceProperty, std::move(device_dictionary));
}

bool IsGuestModeActive() {
  return user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
         user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
}

// Get the euicc path for reset euicc operation. Return std::nullopt if the
// reset euicc is not allowed, i.e: the user is in guest mode, admin enables
// restrict cellular network policy or a managed eSIM profile already installed.
std::optional<dbus::ObjectPath> GetEuiccResetPath() {
  if (IsGuestModeActive()) {
    NET_LOG(ERROR) << "Couldn't reset EUICC in guest mode.";
    return std::nullopt;
  }
  std::optional<dbus::ObjectPath> euicc_path =
      cellular_utils::GetCurrentEuiccPath();
  if (!euicc_path) {
    NET_LOG(ERROR) << "No current EUICC. Unable to reset EUICC";
    return std::nullopt;
  }
  const ManagedNetworkConfigurationHandler*
      managed_network_configuration_handler =
          NetworkHandler::Get()->managed_network_configuration_handler();
  if (!managed_network_configuration_handler)
    return std::nullopt;
  if (managed_network_configuration_handler
          ->AllowOnlyPolicyCellularNetworks()) {
    NET_LOG(ERROR)
        << "Couldn't reset EUICC if admin restricts cellular networks.";
    return std::nullopt;
  }
  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  if (!network_state_handler)
    return std::nullopt;
  NetworkStateHandler::NetworkStateList state_list;
  network_state_handler->GetNetworkListByType(NetworkTypePattern::Cellular(),
                                              /*configured_only=*/false,
                                              /*visible_only=*/false,
                                              /*limit=*/0, &state_list);

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(*euicc_path);
  const std::string& eid = euicc_properties->eid().value();
  for (const NetworkState* network : state_list) {
    if (network->eid() == eid && network->IsManagedByPolicy()) {
      NET_LOG(ERROR)
          << "Couldn't reset EUICC if a managed eSIM profile is installed.";
      return std::nullopt;
    }
  }

  return euicc_path;
}

std::string HexDecode(const std::string& hex_ssid) {
  std::string ssid;
  if (!base::HexStringToString(hex_ssid, &ssid)) {
    NET_LOG(ERROR) << "Error decoding HexSSID: " << hex_ssid;
  }

  return ssid;
}

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
  void OpenFeedbackDialog(const base::Value::List& value) {
    chrome::ShowFeedbackPage(
        nullptr, feedback::kFeedbackSourceNetworkHealthPage,
        "" /*description_template*/, "" /*description_template_placeholder*/,
        "network-health", "" /*extra_diagnostics*/);
  }
};

}  // namespace

namespace network_ui {

class NetworkConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkConfigMessageHandler() {}

  NetworkConfigMessageHandler(const NetworkConfigMessageHandler&) = delete;
  NetworkConfigMessageHandler& operator=(const NetworkConfigMessageHandler&) =
      delete;

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
        kGetFirstWifiNetworkProperties,
        base::BindRepeating(
            &NetworkConfigMessageHandler::GetFirstWifiNetworkProperties,
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
        kResetESimCache,
        base::BindRepeating(&NetworkConfigMessageHandler::ResetESimCache,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kDisableESimProfile,
        base::BindRepeating(
            &NetworkConfigMessageHandler::DisableActiveESimProfile,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kResetEuicc,
        base::BindRepeating(&NetworkConfigMessageHandler::ResetEuicc,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kResetApnMigrator,
        base::BindRepeating(&NetworkConfigMessageHandler::ResetApnMigrator,
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
  void Respond(const std::string& callback_id, const base::ValueView response) {
    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), response);
  }

  void GetShillNetworkProperties(const base::Value::List& arg_list) {
    CHECK_EQ(2u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();
    std::string guid = arg_list[1].GetString();
    ProvideNetworkProperties(callback_id, guid);
  }

  void GetFirstWifiNetworkProperties(const base::Value::List& arg_list) {
    std::string callback_id = arg_list[0].GetString();
    const NetworkState* wifi_network =
        NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
            NetworkTypePattern::WiFi());
    if (wifi_network) {
      ProvideNetworkProperties(callback_id, wifi_network->guid());
      return;
    }
    Respond(callback_id, base::Value::List());
  }

  void ProvideNetworkProperties(const std::string& callback_id,
                                const std::string& guid) {
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
                                   std::optional<base::Value::Dict> result) {
    if (!result) {
      RunErrorCallback(callback_id, guid, kGetNetworkProperties, "Error.DBus");
      return;
    }
    // Set the 'service_path' property for debugging.
    result->Set("service_path", service_path);
    // Set the device properties for debugging.
    SetDeviceProperties(&result.value());
    base::Value::List return_arg_list;
    return_arg_list.Append(std::move(*result));
    Respond(callback_id, return_arg_list);
  }

  void GetShillDeviceProperties(const base::Value::List& arg_list) {
    CHECK_EQ(2u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();
    std::string type = arg_list[1].GetString();

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

  void GetShillEthernetEAP(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    NetworkStateHandler::NetworkStateList list;
    NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
        NetworkTypePattern::Primitive(shill::kTypeEthernetEap),
        true /* configured_only */, false /* visible_only */, 1 /* limit */,
        &list);

    if (list.empty()) {
      Respond(callback_id, base::Value::List());
      return;
    }
    const NetworkState* eap = list.front();

    Respond(callback_id,
            base::Value::List().Append(base::Value::Dict()
                                           .Set("guid", eap->guid())
                                           .Set("name", eap->name())
                                           .Set("type", eap->type())));
  }

  void OpenCellularActivationUi(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    const NetworkState* cellular_network =
        NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
            NetworkTypePattern::Cellular());
    if (cellular_network) {
      SystemTrayClientImpl::Get()->ShowSettingsCellularSetup(
          /*show_psim_flow=*/true);
    }
    base::Value::List response;
    response.Append(base::Value(cellular_network != nullptr));
    Respond(callback_id, response);
  }

  void ResetESimCache(const base::Value::List& arg_list) {
    CellularESimProfileHandler* handler =
        NetworkHandler::Get()->cellular_esim_profile_handler();
    if (!handler)
      return;

    CellularESimProfileHandlerImpl* handler_impl =
        static_cast<CellularESimProfileHandlerImpl*>(handler);
    handler_impl->ResetESimProfileCache();
  }

  void DisableActiveESimProfile(const base::Value::List& arg_list) {
    CellularESimProfileHandler* handler =
        NetworkHandler::Get()->cellular_esim_profile_handler();
    if (!handler)
      return;

    CellularESimProfileHandlerImpl* handler_impl =
        static_cast<CellularESimProfileHandlerImpl*>(handler);
    handler_impl->DisableActiveESimProfile();
  }

  void ResetEuicc(const base::Value::List& arg_list) {
    std::optional<dbus::ObjectPath> euicc_path = GetEuiccResetPath();
    if (!euicc_path)
      return;

    CellularESimUninstallHandler* handler =
        NetworkHandler::Get()->cellular_esim_uninstall_handler();
    if (!handler)
      return;
    NET_LOG(EVENT) << "Executing reset EUICC on " << euicc_path->value();
    handler->ResetEuiccMemory(
        *euicc_path, base::BindOnce(&NetworkConfigMessageHandler::OnEuiccReset,
                                    base::Unretained(this)));
  }

  void ResetApnMigrator(const base::Value::List& arg_list) {
    NET_LOG(EVENT) << "Executing reset ApnMigrator";
    PrefService* local_state = g_browser_process->local_state();

    // Clear set of migrated ICCIDs.
    local_state->ClearPref(prefs::kApnMigratedIccids);

    // Clear all revamp APN lists in all network.
    const std::string network_metadata_pref = "network_metadata";
    base::Value::Dict device_dict =
        local_state->GetDict(network_metadata_pref).Clone();
    for (auto const [guid, val] : device_dict) {
      base::Value::Dict* network_dict = device_dict.FindDict(guid.c_str());
      network_dict->Remove("custom_apn_list_v2");
    }
    local_state->SetDict(network_metadata_pref, std::move(device_dict));
  }

  void OnEuiccReset(bool success) {
    if (!success) {
      NET_LOG(ERROR) << "Error occurred when resetting EUICC.";
    }
  }

  void ShowNetworkDetails(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string guid = arg_list[0].GetString();

    InternetDetailDialog::ShowDialog(guid);
  }

  void ShowNetworkConfig(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string guid = arg_list[0].GetString();

    InternetConfigDialog::ShowDialogForNetworkId(guid);
  }

  void ShowAddNewWifi(const base::Value::List& arg_list) {
    InternetConfigDialog::ShowDialogForNetworkType(::onc::network_type::kWiFi);
  }

  void OnGetShillDeviceProperties(const std::string& callback_id,
                                  const std::string& type,
                                  const std::string& device_path,
                                  std::optional<base::Value::Dict> result) {
    if (!result) {
      RunErrorCallback(callback_id, type, kGetDeviceProperties,
                       "GetDeviceProperties failed");
      return;
    }

    // Set the 'device_path' property for debugging.
    result->Set("device_path", device_path);

    base::Value::List return_arg_list;
    return_arg_list.Append(std::move(*result));
    Respond(callback_id, return_arg_list);
  }

  void GetHostname(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();
    std::string hostname =
        NetworkHandler::Get()->network_state_handler()->hostname();
    Respond(callback_id, base::Value(hostname));
  }

  void SetHostname(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string hostname = arg_list[0].GetString();
    NET_LOG(USER) << "SET HOSTNAME: " << hostname;
    NetworkHandler::Get()->network_state_handler()->SetHostname(hostname);
  }

  void RunErrorCallback(const std::string& callback_id,
                        const std::string& guid_or_type,
                        const std::string& function_name,
                        const std::string& error_name) {
    NET_LOG(ERROR) << "Shill Error: " << error_name << " id=" << guid_or_type;
    std::string key = function_name == kGetDeviceProperties
                          ? shill::kTypeProperty
                          : shill::kGuidProperty;

    Respond(callback_id, base::Value::List().Append(
                             base::Value::Dict()
                                 .Set(key, base::Value(guid_or_type))
                                 .Set("ShillError", base::Value(error_name))));
  }

  void AddNetwork(const base::Value::List& args) {
    DCHECK(!args.empty());
    std::string onc_type = args[0].GetString();
    InternetConfigDialog::ShowDialogForNetworkType(onc_type);
  }

  base::WeakPtrFactory<NetworkConfigMessageHandler> weak_ptr_factory_{this};
};

class HotspotConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  HotspotConfigMessageHandler() = default;
  ~HotspotConfigMessageHandler() override = default;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        kGetTetheringCapabilities,
        base::BindRepeating(
            &HotspotConfigMessageHandler::GetTetheringCapabilities,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetTetheringStatus,
        base::BindRepeating(&HotspotConfigMessageHandler::GetTetheringStatus,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetTetheringConfig,
        base::BindRepeating(&HotspotConfigMessageHandler::GetTetheringConfig,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kSetTetheringConfig,
        base::BindRepeating(&HotspotConfigMessageHandler::SetTetheringConfig,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kCheckTetheringReadiness,
        base::BindRepeating(
            &HotspotConfigMessageHandler::CheckTetheringReadiness,
            base::Unretained(this)));
  }

 private:
  void Respond(const std::string& callback_id, const base::ValueView response) {
    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), response);
  }

  void GetTetheringCapabilities(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    ShillManagerClient::Get()->GetProperties(base::BindOnce(
        &HotspotConfigMessageHandler::OnGetShillManagerDictPropertiesByKey,
        weak_ptr_factory_.GetWeakPtr(), callback_id,
        shill::kTetheringCapabilitiesProperty));
  }

  void GetTetheringStatus(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    ShillManagerClient::Get()->GetProperties(base::BindOnce(
        &HotspotConfigMessageHandler::OnGetShillManagerDictPropertiesByKey,
        weak_ptr_factory_.GetWeakPtr(), callback_id,
        shill::kTetheringStatusProperty));
  }

  void GetTetheringConfig(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    ShillManagerClient::Get()->GetProperties(base::BindOnce(
        &HotspotConfigMessageHandler::OnGetShillManagerDictPropertiesByKey,
        weak_ptr_factory_.GetWeakPtr(), callback_id,
        shill::kTetheringConfigProperty));
  }

  void CheckTetheringReadiness(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    ShillManagerClient::Get()->CheckTetheringReadiness(
        base::BindOnce(&HotspotConfigMessageHandler::RespondStringResult,
                       weak_ptr_factory_.GetWeakPtr(), callback_id),
        base::BindOnce(&HotspotConfigMessageHandler::RespondError,
                       weak_ptr_factory_.GetWeakPtr(), callback_id,
                       kCheckTetheringReadiness));
  }

  void SetTetheringConfig(const base::Value::List& arg_list) {
    CHECK_EQ(2u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();
    std::string tethering_config = arg_list[1].GetString();
    std::optional<base::Value> value = base::JSONReader::Read(tethering_config);

    if (!value || !value->is_dict()) {
      NET_LOG(ERROR) << "Invalid tethering configuration: " << tethering_config;
      Respond(callback_id, base::Value("Invalid tethering configuration"));
      return;
    }
    NET_LOG(USER) << "SetManagerProperty: " << shill::kTetheringConfigProperty
                  << ": " << *value;
    const std::string* ssid =
        value->GetDict().FindString(shill::kTetheringConfSSIDProperty);
    if (ssid) {
      value->GetDict().Set(shill::kTetheringConfSSIDProperty,
                           base::Value(base::HexEncode(*ssid)));
    }

    ShillManagerClient::Get()->SetProperty(
        shill::kTetheringConfigProperty, *value,
        base::BindOnce(&HotspotConfigMessageHandler::RespondStringResult,
                       weak_ptr_factory_.GetWeakPtr(), callback_id, "success"),
        base::BindOnce(
            &HotspotConfigMessageHandler::SetManagerPropertiesErrorCallback,
            weak_ptr_factory_.GetWeakPtr(), callback_id,
            shill::kTetheringConfigProperty));
  }

  void OnGetShillManagerDictPropertiesByKey(
      const std::string& callback_id,
      const std::string& dict_key,
      std::optional<base::Value::Dict> properties) {
    if (!properties) {
      NET_LOG(ERROR) << "Error getting Shill manager properties.";
      Respond(callback_id,
              base::Value("Error getting Shill manager properties."));
      return;
    }

    base::Value::Dict* value = properties->FindDict(dict_key);
    if (value) {
      const std::string* ssid =
          value->FindString(shill::kTetheringConfSSIDProperty);
      if (ssid) {
        value->Set(shill::kTetheringConfSSIDProperty, HexDecode(*ssid));
      }
      Respond(callback_id, *value);
      return;
    }

    Respond(callback_id, base::Value::Dict());
  }

  void SetManagerPropertiesErrorCallback(
      const std::string& callback_id,
      const std::string& property_name,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message) {
    NET_LOG(ERROR) << "Error setting Shill manager properties: "
                   << property_name << ", error: " << dbus_error_name
                   << ", message: " << dbus_error_message;
    Respond(callback_id, base::Value(dbus_error_name));
  }

  void RespondError(const std::string& callback_id,
                    const std::string& operation,
                    const std::string& error_name,
                    const std::string& error_message) {
    NET_LOG(ERROR) << "Error occured when " << operation << ": " << error_name
                   << ", error message: " << error_message;
    Respond(callback_id, base::Value(error_name));
  }

  void RespondStringResult(const std::string& callback_id,
                           const std::string& result) {
    Respond(callback_id, base::Value(result));
  }

  base::WeakPtrFactory<HotspotConfigMessageHandler> weak_ptr_factory_{this};
};

class WifiDirectMessageHandler : public content::WebUIMessageHandler {
 public:
  WifiDirectMessageHandler() = default;
  ~WifiDirectMessageHandler() override = default;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        kGetWifiDirectCapabilities,
        base::BindRepeating(
            &WifiDirectMessageHandler::GetWifiDirectCapabilities,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetWifiDirectOwnerInfo,
        base::BindRepeating(&WifiDirectMessageHandler::GetWifiDirectOwnerInfo,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetWifiDirectClientInfo,
        base::BindRepeating(&WifiDirectMessageHandler::GetWifiDirectClientInfo,
                            base::Unretained(this)));
  }

 private:
  void Respond(const std::string& callback_id, const base::ValueView response) {
    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), response);
  }

  void GetWifiDirectCapabilities(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    ShillManagerClient::Get()->GetProperties(base::BindOnce(
        &WifiDirectMessageHandler::OnGetShillManagerDictPropertiesByKey,
        weak_ptr_factory_.GetWeakPtr(), callback_id,
        shill::kP2PCapabilitiesProperty));
  }

  void GetWifiDirectOwnerInfo(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    ShillManagerClient::Get()->GetProperties(base::BindOnce(
        &WifiDirectMessageHandler::OnGetShillManagerListPropertiesByKey,
        weak_ptr_factory_.GetWeakPtr(), callback_id,
        shill::kP2PGroupInfosProperty));
  }

  void GetWifiDirectClientInfo(const base::Value::List& arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    ShillManagerClient::Get()->GetProperties(base::BindOnce(
        &WifiDirectMessageHandler::OnGetShillManagerListPropertiesByKey,
        weak_ptr_factory_.GetWeakPtr(), callback_id,
        shill::kP2PClientInfosProperty));
  }

  void OnGetShillManagerListPropertiesByKey(
      const std::string& callback_id,
      const std::string& dict_key,
      std::optional<base::Value::Dict> properties) {
    if (!properties) {
      NET_LOG(ERROR) << "Error getting Shill manager properties.";
      Respond(callback_id,
              base::Value("Error getting Shill manager properties."));
      return;
    }

    base::Value::List* value = properties->FindList(dict_key);
    Respond(callback_id, value ? std::move(*value) : base::Value::List());
  }

  void OnGetShillManagerDictPropertiesByKey(
      const std::string& callback_id,
      const std::string& dict_key,
      std::optional<base::Value::Dict> properties) {
    if (!properties) {
      NET_LOG(ERROR) << "Error getting Shill manager properties.";
      Respond(callback_id,
              base::Value("Error getting Shill manager properties."));
      return;
    }

    base::Value::Dict* value = properties->FindDict(dict_key);
    Respond(callback_id, value ? std::move(*value) : base::Value::Dict());
  }

  base::WeakPtrFactory<WifiDirectMessageHandler> weak_ptr_factory_{this};
};

}  // namespace network_ui

// static
base::Value::Dict NetworkUI::GetLocalizedStrings() {
  return base::Value::Dict()
      .Set("titleText", l10n_util::GetStringUTF16(IDS_NETWORK_UI_TITLE))
      .Set("generalTab", l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_GENERAL))
      .Set("networkHealthTab",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_HEALTH))
      .Set("networkLogsTab",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_LOGS))
      .Set("networkStateTab",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_STATE))
      .Set("networkSelectTab",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_SELECT))
      .Set("networkHotspotTab",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_HOTSPOT))
      .Set("networkMetricsTab",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_METRICS))
      .Set("networkWifiDirectTab",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_WIFI_DIRECT))
      .Set("autoRefreshText",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_AUTO_REFRESH))
      .Set("deviceLogLinkText",
           l10n_util::GetStringUTF16(IDS_DEVICE_LOG_LINK_TEXT))
      .Set("networkRefreshText",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_REFRESH))
      .Set("clickToExpandText",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_EXPAND))
      .Set("propertyFormatText",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_PROPERTY_FORMAT))
      .Set("normalFormatOption",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_NORMAL))
      .Set("managedFormatOption",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_MANAGED))
      .Set("stateFormatOption",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_STATE))
      .Set("shillFormatOption",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_SHILL))
      .Set("dhcpHostnameLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_DHCP_HOSTNAME))
      .Set("globalPolicyLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_GLOBAL_POLICY))
      .Set("networkListsLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LISTS))
      .Set("networkHealthLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_HEALTH))
      .Set("networkDiagnosticsLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_DIAGNOSTICS))
      .Set("visibleNetworksLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_VISIBLE_NETWORKS))
      .Set("favoriteNetworksLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_FAVORITE_NETWORKS))
      .Set("ethernetEapNetworkLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_ETHERNET_EAP))
      .Set("devicesLabel", l10n_util::GetStringUTF16(IDS_NETWORK_UI_DEVICES))
      .Set("cellularActivationLabel",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NO_CELLULAR_ACTIVATION_LABEL))
      .Set("cellularActivationButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_OPEN_CELLULAR_ACTIVATION_BUTTON_TEXT))
      .Set("noCellularErrorText",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NO_CELLULAR_ERROR_TEXT))
      .Set("resetESimCacheLabel",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_RESET_ESIM_PROFILES_BUTTON_TEXT))
      .Set("resetESimCacheButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_RESET_ESIM_PROFILES_BUTTON_TEXT))
      .Set(
          "disableESimProfilesLabel",
          l10n_util::GetStringUTF16(IDS_NETWORK_UI_DISABLE_ESIM_PROFILES_LABEL))
      .Set("disableActiveESimProfileButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_DISABLE_ACTIVE_ESIM_PROFILE_BUTTON_TEXT))
      .Set("resetEuiccLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_RESET_EUICC_LABEL))
      .Set("resetApnMigratorLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_RESET_APN_MIGRATOR_LABEL))
      .Set("addNewWifiLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_ADD_NEW_WIFI_LABEL))
      .Set("addNewWifiButtonText",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_ADD_NEW_WIFI_BUTTON_TEXT))
      .Set("importOncButtonText",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_IMPORT_ONC_BUTTON_TEXT))
      .Set("addWiFiListItemName",
           l10n_util::GetStringUTF16(IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME))

      // Network logs
      .Set("networkLogsDescription",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DESCRIPTION))
      .Set("networkLogsSystemLogs",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_SYSTEM_LOGS))
      .Set("networkLogsFilterPii",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_FILTER_PII))
      .Set("networkLogsPolicies",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_POLICIES))
      .Set("networkLogsDebugLogs",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUG_LOGS))
      .Set("networkLogsChromeLogs",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_CHROME_LOGS))
      .Set("networkLogsStoreButton",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_STORE_BUTTON))
      .Set("networkLogsStatus",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_STATUS))
      .Set("networkLogsDebuggingTitle",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_TITLE))
      .Set("networkLogsDebuggingDescription",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_DESCRIPTION))
      .Set(
          "networkLogsDebuggingNone",
          l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_NONE))
      .Set("networkLogsDebuggingUnknown",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_UNKNOWN))

      // Network Diagnostics
      .Set("NetworkDiagnosticsRunAll",
           l10n_util::GetStringUTF16(IDS_NETWORK_DIAGNOSTICS_RUN_ALL))
      .Set("NetworkDiagnosticsSendFeedback",
           l10n_util::GetStringUTF16(IDS_NETWORK_DIAGNOSTICS_SEND_FEEDBACK))
      .Set("renderNetworkSelectButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_RENDER_NETWORK_SELECT_BUTTON_TEXT))
      .Set("renderNetworkSelectButtonDescription",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_RENDER_NETWORK_SELECT_BUTTON_DESCRIPTION))

      // Network Metrics
      .Set("networkMetricsLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_METRICS_LABEL))
      .Set("renderGraphButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NETWORK_METRICS_RENDER_BUTTON))
      .Set("startPlotsButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NETWORK_METRICS_START_BUTTON))
      .Set(
          "stopPlotsButtonText",
          l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_METRICS_STOP_BUTTON))
      .Set("increaseRateButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NETWORK_METRICS_INCREASE_RATE_BUTTON))
      .Set("decreaseRateButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_NETWORK_METRICS_DECREASE_RATE_BUTTON))

      // Network Hotspot
      .Set("tetheringCapabilitiesLabel",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_TETHERING_CAPABILITIES_LABEL))
      .Set("refreshTetheringCapabilitiesButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_REFRESH_TETHERING_CAPABILITIES_BUTTON_TEXT))
      .Set("tetheringStatusLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TETHERING_STATUS_LABEL))
      .Set("refreshTetheringStatusButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_REFRESH_TETHERING_STATUS_BUTTON_TEXT))
      .Set("tetheringConfigLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TETHERING_CONFIG_LABEL))
      .Set("refreshTetheringConfigButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_REFRESH_TETHERING_CONFIG_BUTTON_TEXT))
      .Set("setTetheringConfigButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_SET_TETHERING_CONFIG_BUTTON_TEXT))
      .Set("tetheringReadinessLabel",
           l10n_util::GetStringUTF16(IDS_NETWORK_UI_TETHERING_READINESS_LABEL))
      .Set("checkTetheringReadinessButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_CHECK_TETHERING_READINESS_BUTTON_TEXT))
      .Set(
          "setTetheringEnabledLabel",
          l10n_util::GetStringUTF16(IDS_NETWORK_UI_SET_TETHERING_ENABLED_LABEL))

      // Network Wifi Direct
      .Set("wifiDirectCapabilitiesLabel",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_WIFI_DIRECT_CAPABILITIES_LABEL))
      .Set("refreshWifiDirectCapabilitiesButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_REFRESH_WIFI_DIRECT_CAPABILITIES_BUTTON_TEXT))
      .Set("wifiDirectOwnerInfoLabel",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_WIFI_DIRECT_OWNER_INFO_LABEL))
      .Set("refreshWifiDirectOwnerInfoButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_REFRESH_WIFI_DIRECT_OWNER_INFO_BUTTON_TEXT))
      .Set("wifiDirectClientInfoLabel",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_WIFI_DIRECT_CLIENT_INFO_LABEL))
      .Set("refreshWifiDirectClientInfoButtonText",
           l10n_util::GetStringUTF16(
               IDS_NETWORK_UI_REFRESH_WIFI_DIRECT_CLIENT_INFO_BUTTON_TEXT));
}

NetworkUI::NetworkUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  web_ui->AddMessageHandler(
      std::make_unique<network_ui::NetworkConfigMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<OncImportMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<NetworkLogsMessageHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<NetworkDiagnosticsMessageHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<network_ui::HotspotConfigMessageHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<network_ui::WifiDirectMessageHandler>());

  // Enable extension API calls in the WebUI.
  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());

  base::Value::Dict localized_strings = GetLocalizedStrings();

  content::WebUIDataSource* html = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUINetworkHost);

  html->AddLocalizedStrings(localized_strings);
  html->AddBoolean("isGuestModeActive", IsGuestModeActive());
  html->AddBoolean("isWifiDirectEnabled", features::IsWifiDirectEnabled());
  html->AddString("tetheringStateStarting", shill::kTetheringStateStarting);
  html->AddString("tetheringStateActive", shill::kTetheringStateActive);
  network_health::AddResources(html);
  network_diagnostics::AddResources(html);
  cellular_setup::AddLocalizedStrings(html);
  cellular_setup::AddNonStringLoadTimeData(html);
  ui::network_element::AddLocalizedStrings(html);
  ui::network_element::AddOncLocalizedStrings(html);
  traffic_counters::AddResources(html);

  webui::SetupWebUIDataSource(
      html, base::make_span(kNetworkUiResources, kNetworkUiResourcesSize),
      IDR_NETWORK_UI_NETWORK_HTML);
  // Enabling trusted types via trusted_types_util must be done after
  // webui::SetupWebUIDataSource to override the trusted type CSP with correct
  // policies for JS WebUIs.
  ash::EnableTrustedTypesCSP(html);
}

NetworkUI::~NetworkUI() = default;

void NetworkUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_health::mojom::NetworkHealthService>
        receiver) {
  network_health::NetworkHealthManager::GetInstance()->BindHealthReceiver(
      std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<
        chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        receiver) {
  network_health::NetworkHealthManager::GetInstance()->BindDiagnosticsReceiver(
      std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver) {
  GetESimManager(std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        receiver) {
  GetPasspointService(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(NetworkUI)

}  // namespace ash
