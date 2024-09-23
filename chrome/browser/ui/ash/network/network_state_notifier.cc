// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network/network_state_notifier.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_name_util.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/shill_error.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

const int kMinTimeBetweenOutOfCreditsNotifySeconds = 10 * 60;

const char kNotifierNetwork[] = "ash.network";
const char kNotifierNetworkError[] = "ash.network.error";

// Ignore in-progress errors and disconnect errors (which may occur when a new
// connect request occurs while a previous connect is in-progress).
bool ShillErrorIsIgnored(const std::string& shill_error) {
  return shill_error == shill::kErrorResultInProgress ||
         shill_error == shill::kErrorDisconnect;
}

// Returns true if |shill_error| is known to be a configuration error.
bool IsConfigurationError(const std::string& shill_error) {
  if (shill_error.empty())
    return false;
  return shill_error == shill::kErrorPinMissing ||
         shill_error == shill::kErrorBadPassphrase ||
         shill_error == shill::kErrorResultInvalidPassphrase ||
         shill_error == shill::kErrorBadWEPKey;
}

std::string GetStringFromDictionary(
    const std::optional<base::Value::Dict>& dict,
    const std::string& key) {
  const std::string* v = dict ? dict->FindString(key) : nullptr;
  return v ? *v : std::string();
}

// Error messages based on |error_name|, not network_state->GetError().
std::u16string GetConnectErrorString(const std::string& error_name) {
  if (error_name == NetworkConnectionHandler::kErrorNotFound)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error_name == NetworkConnectionHandler::kErrorConfigureFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CONFIGURE_FAILED);
  }
  if (error_name == NetworkConnectionHandler::kErrorCertLoadTimeout) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CERTIFICATES_NOT_LOADED);
  }
  if (error_name == NetworkConnectionHandler::kErrorActivateFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  }
  if (error_name == NetworkConnectionHandler::kErrorSimPinPukLocked) {
    return l10n_util::GetStringUTF16(IDS_NETWORK_LIST_SIM_CARD_LOCKED);
  }
  if (error_name == NetworkConnectionHandler::kErrorSimCarrierLocked) {
    return l10n_util::GetStringUTF16(IDS_NETWORK_LIST_SIM_CARD_CARRIER_LOCKED);
  }
  return std::u16string();
}

const gfx::VectorIcon& GetErrorNotificationVectorIcon(
    const std::string& network_type) {
  if (network_type == shill::kTypeVPN)
    return kNotificationVpnIcon;
  if (network_type == shill::kTypeCellular)
    return kNotificationMobileDataOffIcon;
  return kNotificationWifiOffIcon;
}

// |identifier| may be a service path or guid.
void ShowErrorNotification(const std::string& identifier,
                           const std::string& notification_id,
                           const NotificationCatalogName& catalog_name,
                           const std::string& network_type,
                           const std::u16string& title,
                           const std::u16string& message,
                           base::RepeatingClosure callback) {
  NET_LOG(ERROR) << "ShowErrorNotification: " << identifier << ": "
                 << base::UTF16ToUTF8(title);
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierNetworkError, catalog_name),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(std::move(callback)),
      GetErrorNotificationVectorIcon(network_type),
      message_center::SystemNotificationWarningLevel::WARNING);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

bool ShouldConnectFailedNotificationBeShown(const std::string& error_name,
                                            const NetworkState* network_state) {
  // Only show a notification for certain errors. Other failures are expected
  // to be handled by the UI that initiated the connect request.
  // Note: kErrorConnectFailed may also cause the configure dialog to be
  // displayed, but we rely on the notification system to show additional
  // details if available.
  if (error_name != NetworkConnectionHandler::kErrorConnectFailed &&
      error_name != NetworkConnectionHandler::kErrorNotFound &&
      error_name != NetworkConnectionHandler::kErrorConfigureFailed &&
      error_name != NetworkConnectionHandler::kErrorCertLoadTimeout &&
      error_name != NetworkConnectionHandler::kErrorSimPinPukLocked &&
      error_name != NetworkConnectionHandler::kErrorSimCarrierLocked) {
    return false;
  }

  // When a connection to a Tether network fails, the Tether component shows its
  // own error notification. If this is the case, there is no need to show an
  // additional notification for the failure to connect to the underlying Wi-Fi
  // network.
  if (network_state && !network_state->tether_guid().empty())
    return false;

  // Otherwise, the connection failed notification should be shown.
  return true;
}

const NetworkState* GetNetworkStateForGuid(const std::string& guid) {
  return guid.empty() ? nullptr
                      : NetworkHandler::Get()
                            ->network_state_handler()
                            ->GetNetworkStateFromGuid(guid);
}

bool IsSimLockConnectionFailure(const std::string& connection_error_name,
                                const NetworkState* network_state) {
  if (connection_error_name ==
      NetworkConnectionHandler::kErrorSimPinPukLocked) {
    return true;
  }

  return network_state && network_state->GetError() == shill::kErrorSimLocked;
}

}  // namespace

const char NetworkStateNotifier::kNetworkConnectNotificationId[] =
    "chrome://settings/internet/connect";
const char NetworkStateNotifier::kNetworkActivateNotificationId[] =
    "chrome://settings/internet/activate";
const char NetworkStateNotifier::kNetworkOutOfCreditsNotificationId[] =
    "chrome://settings/internet/out-of-credits";
const char NetworkStateNotifier::kNetworkCarrierUnlockNotificationId[] =
    "chrome://settings/internet/carrier-unlock";

NetworkStateNotifier::NetworkStateNotifier() {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  network_state_handler_observer_.Observe(handler);
  NetworkStateHandler::NetworkStateList active_networks;
  handler->GetActiveNetworkListByType(NetworkTypePattern::Default(),
                                      &active_networks);
  ActiveNetworksChanged(active_networks);
  NetworkHandler::Get()->network_connection_handler()->AddObserver(this);
}

NetworkStateNotifier::~NetworkStateNotifier() {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkHandler::Get()->network_connection_handler()->RemoveObserver(this);
}

void NetworkStateNotifier::ConnectToNetworkRequested(
    const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (network && network->type() == shill::kTypeVPN)
    connected_vpn_.reset();

  RemoveConnectNotification();
}

void NetworkStateNotifier::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!network->IsConnectedState() ||
      connect_error_notification_network_guid_.empty() ||
      connect_error_notification_network_guid_ != network->guid()) {
    return;
  }
  RemoveConnectNotification();
}

void NetworkStateNotifier::NetworkIdentifierTransitioned(
    const std::string& old_service_path,
    const std::string& new_service_path,
    const std::string& old_guid,
    const std::string& new_guid) {
  if (old_guid == new_guid ||
      old_guid != connect_error_notification_network_guid_) {
    return;
  }

  connect_error_notification_network_guid_ = new_guid;
}

void NetworkStateNotifier::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void NetworkStateNotifier::ConnectSucceeded(const std::string& service_path) {
  RemoveConnectNotification();
}

void NetworkStateNotifier::ConnectFailed(const std::string& service_path,
                                         const std::string& error_name) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!ShouldConnectFailedNotificationBeShown(error_name, network)) {
    NET_LOG(EVENT) << "Skipping notification for: "
                   << NetworkPathId(service_path) << " Error: " << error_name;
    return;
  }
  ShowNetworkConnectErrorForGuid(error_name, network ? network->guid() : "");
}

void NetworkStateNotifier::DisconnectRequested(
    const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (network && network->type() == shill::kTypeVPN)
    connected_vpn_.reset();
}

void NetworkStateNotifier::ActiveNetworksChanged(
    const std::vector<const NetworkState*>& active_networks) {
  const NetworkState* active_vpn = nullptr;
  std::string active_non_vpn_network_guid;
  // NOTE: The list of 'active' networks includes disconnected networks that
  // were connected or connecting.
  for (const auto* network : active_networks) {
    if (network->type() == shill::kTypeVPN) {
      // Make sure that if there is an edge case with two active VPNs that we
      // track the first active one.
      if (!active_vpn)
        active_vpn = network;
    } else if (active_non_vpn_network_guid.empty()) {
      // We are only interested in the "default" (first active) non virtual
      // network.
      if (network->IsConnectingOrConnected())
        active_non_vpn_network_guid = network->guid();
    }
  }
  UpdateVpnConnectionState(active_vpn);

  // If the default network changes, allow the out of credits notification to be
  // shown again. A delay prevents the notification from being shown too
  // frequently (see below).
  if (active_non_vpn_network_guid != active_non_vpn_network_guid_) {
    active_non_vpn_network_guid_ = active_non_vpn_network_guid;
    did_show_out_of_credits_ = false;
    UpdateCellularOutOfCredits();
  }
}

void NetworkStateNotifier::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (network->type() != shill::kTypeCellular)
    return;
  if (network->cellular_out_of_credits())
    UpdateCellularOutOfCredits();
  UpdateCellularActivating(network);
}

void NetworkStateNotifier::UpdateVpnConnectionState(
    const NetworkState* active_vpn) {
  if (!active_vpn || !active_vpn->IsConnectingOrConnected()) {
    // No connecting or connected VPN. If we were tracking a connected VPN,
    // show a notification.
    if (connected_vpn_) {
      ShowVpnDisconnectedNotification(connected_vpn_.get());
      connected_vpn_.reset();
    }
    return;
  }
  if (connected_vpn_ && active_vpn->guid() == connected_vpn_->guid) {
    // The connected VPN is unchanged and still connected or connecting. If the
    // VPN goes from connected -> connecting -> connected, we do not want to
    // show a notification.
    return;
  }

  // Do not track ARC VPNs for showing disconnect notifications. Also make sure
  // |connected_vpn_| is cleared when connected to an ARC VPN.
  if (active_vpn->GetVpnProviderType() == shill::kProviderArcVpn) {
    connected_vpn_.reset();
    return;
  }

  // If we are connected to a new VPN, track it as the connected VPN. Do not
  // show a notification, even if we were tracking a different connected VPN,
  // since we are still connected to a VPN (i.e. just replace |connected_vpn_|).
  if (active_vpn->IsConnectedState()) {
    connected_vpn_ =
        std::make_unique<VpnDetails>(active_vpn->guid(), active_vpn->name());
  } else {
    // |active_vpn| is connecting. Clear |connected_vpn_| so that we do not
    // show a notification for the previous VPN.
    connected_vpn_.reset();
  }
}

void NetworkStateNotifier::UpdateCellularOutOfCredits() {
  // Only show the notification once (reset when the primary network changes).
  if (did_show_out_of_credits_)
    return;

  NetworkStateHandler::NetworkStateList active_networks;
  NetworkHandler::Get()->network_state_handler()->GetActiveNetworkListByType(
      NetworkTypePattern::NonVirtual(), &active_networks);
  const NetworkState* primary_network = nullptr;
  for (const auto* network : active_networks) {
    // Don't display notification if any network is connecting.
    if (network->IsConnectingState())
      return;
    if (!primary_network)
      primary_network = network;
  }

  if (!primary_network ||
      !primary_network->Matches(NetworkTypePattern::Cellular()) ||
      !primary_network->cellular_out_of_credits()) {
    return;
  }

  did_show_out_of_credits_ = true;
  base::TimeDelta dtime = base::Time::Now() - out_of_credits_notify_time_;
  if (dtime.InSeconds() > kMinTimeBetweenOutOfCreditsNotifySeconds) {
    out_of_credits_notify_time_ = base::Time::Now();
    std::u16string error_msg =
        l10n_util::GetStringFUTF16(IDS_NETWORK_OUT_OF_CREDITS_BODY,
                                   base::UTF8ToUTF16(primary_network->name()));
    ShowErrorNotification(
        NetworkId(primary_network), kNetworkOutOfCreditsNotificationId,
        NotificationCatalogName::kNetworkOutOfCredits, primary_network->type(),
        l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_CREDITS_TITLE), error_msg,
        base::BindRepeating(&NetworkStateNotifier::ShowCarrierAccountDetail,
                            weak_ptr_factory_.GetWeakPtr(),
                            primary_network->guid()));
  }
}

void NetworkStateNotifier::UpdateCellularActivating(
    const NetworkState* cellular) {
  const std::string cellular_guid = cellular->guid();
  // Keep track of any activating cellular network.
  std::string activation_state = cellular->activation_state();
  if (activation_state == shill::kActivationStateActivating) {
    cellular_activating_guids_.insert(cellular_guid);
    return;
  }
  // Only display a notification if this network was activating and is now
  // activated.
  if (!cellular_activating_guids_.count(cellular_guid) ||
      activation_state != shill::kActivationStateActivated) {
    return;
  }

  cellular_activating_guids_.erase(cellular_guid);
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNetworkActivateNotificationId,
      l10n_util::GetStringUTF16(IDS_NETWORK_CELLULAR_ACTIVATED_TITLE),
      l10n_util::GetStringFUTF16(IDS_NETWORK_CELLULAR_ACTIVATED,
                                 base::UTF8ToUTF16((cellular->name()))),
      std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierNetwork,
          NotificationCatalogName::kNetworkCellularActivated),
      {},
      new message_center::HandleNotificationClickDelegate(
          base::BindRepeating(&NetworkStateNotifier::ShowNetworkSettings,
                              weak_ptr_factory_.GetWeakPtr(), cellular_guid)),
      kNotificationMobileDataIcon,
      message_center::SystemNotificationWarningLevel::WARNING);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void NetworkStateNotifier::ShowNetworkConnectErrorForGuid(
    const std::string& error_name,
    const std::string& guid) {
  const NetworkState* network = GetNetworkStateForGuid(guid);
  if (!network) {
    ShowConnectErrorNotification(error_name,
                                 /*service_path=*/std::string(),
                                 /*shill_properties=*/std::nullopt);
    return;
  }
  // Get the up-to-date properties for the network and display the error.
  NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
      network->path(),
      base::BindRepeating(&NetworkStateNotifier::OnConnectErrorGetProperties,
                          weak_ptr_factory_.GetWeakPtr(), error_name));
}

void NetworkStateNotifier::ShowMobileActivationErrorForGuid(
    const std::string& guid) {
  const NetworkState* cellular = GetNetworkStateForGuid(guid);
  if (!cellular || cellular->type() != shill::kTypeCellular) {
    NET_LOG(ERROR) << "ShowMobileActivationError without Cellular network: "
                   << guid;
    return;
  }
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNetworkActivateNotificationId,
      l10n_util::GetStringUTF16(IDS_NETWORK_ACTIVATION_ERROR_TITLE),
      l10n_util::GetStringFUTF16(IDS_NETWORK_ACTIVATION_NEEDS_CONNECTION,
                                 base::UTF8ToUTF16((cellular->name()))),
      std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierNetworkError,
          NotificationCatalogName::kNetworkActivationError),
      {},
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &NetworkStateNotifier::ShowNetworkSettings,
          weak_ptr_factory_.GetWeakPtr(), cellular->guid())),
      kNotificationMobileDataOffIcon,
      message_center::SystemNotificationWarningLevel::WARNING);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void NetworkStateNotifier::RemoveConnectNotification() {
  SystemNotificationHelper::GetInstance()->Close(kNetworkConnectNotificationId);
  connect_error_notification_network_guid_.clear();
}

void NetworkStateNotifier::RemoveCarrierUnlockNotification() {
  SystemNotificationHelper::GetInstance()->Close(
      kNetworkCarrierUnlockNotificationId);
}

void NetworkStateNotifier::OnConnectErrorGetProperties(
    const std::string& error_name,
    const std::string& service_path,
    std::optional<base::Value::Dict> shill_properties) {
  if (!shill_properties) {
    ShowConnectErrorNotification(error_name, service_path,
                                 std::move(shill_properties));
    return;
  }

  std::string state =
      GetStringFromDictionary(shill_properties, shill::kStateProperty);
  if (NetworkState::StateIsConnected(state) ||
      NetworkState::StateIsConnecting(state)) {
    NET_LOG(EVENT) << "Skipping connect error notification. State: " << state;
    // Network is no longer in an error state. This can happen if an
    // unexpected idle state transition occurs, see http://crbug.com/333955.
    return;
  }
  ShowConnectErrorNotification(error_name, service_path,
                               std::move(shill_properties));
}

void NetworkStateNotifier::ShowConnectErrorNotification(
    const std::string& error_name,
    const std::string& service_path,
    std::optional<base::Value::Dict> shill_properties) {
  std::u16string error = GetConnectErrorString(error_name);
  NET_LOG(DEBUG) << "Notify: " << NetworkPathId(service_path)
                 << ": Connect error: " << error_name << ": "
                 << base::UTF16ToUTF8(error);

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  std::string guid = network ? network->guid() : "";
  std::string log_id =
      network ? NetworkId(network) : NetworkPathId(service_path);

  if (error.empty()) {
    std::string shill_error =
        GetStringFromDictionary(shill_properties, shill::kErrorProperty);
    if (!NetworkState::ErrorIsValid(shill_error)) {
      shill_error = GetStringFromDictionary(shill_properties,
                                            shill::kPreviousErrorProperty);
      NET_LOG(DEBUG) << "Notify: " << log_id
                     << ": Service.PreviousError: " << shill_error;
      if (!NetworkState::ErrorIsValid(shill_error))
        shill_error.clear();
    } else {
      NET_LOG(DEBUG) << "Notify: " << log_id
                     << ": Service.Error: " << shill_error;
    }

    if (network) {
      // Log all error values for debugging.
      NET_LOG(DEBUG) << "Notify: " << log_id
                     << ": Network.GetError(): " << network->GetError()
                     << " shill_connect_error: "
                     << network->shill_connect_error();
      if (shill_error.empty())
        shill_error = network->GetError();
      if (shill_error.empty())
        shill_error = network->shill_connect_error();
    }

    if (ShillErrorIsIgnored(shill_error)) {
      NET_LOG(DEBUG) << "Notify: " << log_id
                     << ": Ignoring error: " << error_name;
      return;
    }
    error = ui::shill_error::GetShillErrorString(shill_error, guid);
    if (error.empty()) {
      if (error_name == NetworkConnectionHandler::kErrorConnectFailed &&
          network && !network->connectable()) {
        // Connect failure on non connectable network with no additional
        // information. We expect the UI to show configuration UI so do not
        // show an additional (and unhelpful) notification.
        return;
      }
      error = l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
    }
  }
  NET_LOG(ERROR) << "Notify: " << log_id
                 << ": Connect error: " + base::UTF16ToUTF8(error);

  CellularESimProfileHandler* cellular_esim_profile_handler =
      NetworkHandler::Get()->cellular_esim_profile_handler();
  std::string network_name;
  if (network) {
    std::optional<std::string> esim_name =
        network_name_util::GetESimProfileName(cellular_esim_profile_handler,
                                              network);
    if (esim_name)
      network_name = *esim_name;
  }
  if (network_name.empty() && shill_properties) {
    network_name = shill_property_util::GetNameFromProperties(
        service_path, shill_properties.value());
  }

  std::string network_error_details =
      GetStringFromDictionary(shill_properties, shill::kErrorDetailsProperty);

  std::u16string error_msg;
  if (!network_error_details.empty()) {
    // network_name shouldn't be empty if network_error_details is set.
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_SERVER_MESSAGE,
        base::UTF8ToUTF16(network_name), error,
        base::UTF8ToUTF16(network_error_details));
  } else if (network_name.empty()) {
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_NO_NAME, error);
  } else {
    error_msg =
        l10n_util::GetStringFUTF16(IDS_NETWORK_CONNECTION_ERROR_MESSAGE,
                                   base::UTF8ToUTF16(network_name), error);
  }

  std::string network_type =
      GetStringFromDictionary(shill_properties, shill::kTypeProperty);

  base::RepeatingClosure on_click;
  if (IsSimLockConnectionFailure(error_name, network)) {
    on_click = base::BindRepeating(&NetworkStateNotifier::ShowSimUnlockSettings,
                                   weak_ptr_factory_.GetWeakPtr());
  } else if (features::IsApnRevampEnabled() && network &&
             network->GetError() == shill::kErrorInvalidAPN) {
    on_click = base::BindRepeating(&NetworkStateNotifier::ShowApnSettings,
                                   weak_ptr_factory_.GetWeakPtr(), guid);
  } else {
    on_click = base::BindRepeating(&NetworkStateNotifier::ShowNetworkSettings,
                                   weak_ptr_factory_.GetWeakPtr(), guid);
  }

  connect_error_notification_network_guid_ = guid;
  ShowErrorNotification(
      NetworkPathId(service_path), kNetworkConnectNotificationId,
      NotificationCatalogName::kNetworkConnectionError, network_type,
      l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE), error_msg,
      std::move(on_click));
}

void NetworkStateNotifier::ShowVpnDisconnectedNotification(VpnDetails* vpn) {
  DCHECK(vpn);
  std::u16string error_msg = l10n_util::GetStringFUTF16(
      IDS_NETWORK_VPN_CONNECTION_LOST_BODY, base::UTF8ToUTF16(vpn->name));
  ShowErrorNotification(
      NetworkGuidId(vpn->guid), kNetworkConnectNotificationId,
      NotificationCatalogName::kNetworkVPNConnectionLost, shill::kTypeVPN,
      l10n_util::GetStringUTF16(IDS_NETWORK_VPN_CONNECTION_LOST_TITLE),
      error_msg,
      base::BindRepeating(&NetworkStateNotifier::ShowNetworkSettings,
                          weak_ptr_factory_.GetWeakPtr(), vpn->guid));
}

void NetworkStateNotifier::ShowCarrierUnlockNotification() {
  std::u16string message =
      l10n_util::GetStringUTF16(IDS_NETWORK_CARRIER_UNLOCK_BODY);
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kNetworkCarrierUnlockNotificationId,
      l10n_util::GetStringFUTF16(IDS_NETWORK_CARRIER_UNLOCK_TITLE,
                                 ui::GetChromeOSDeviceName()),
      message, std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierNetworkError,
          NotificationCatalogName::kNetworkCarrierUnlock),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(
          base::BindRepeating(&NetworkStateNotifier::ShowMobileDataSubpage,
                              weak_ptr_factory_.GetWeakPtr())),
      gfx::kNoneIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void NetworkStateNotifier::ShowNetworkSettings(const std::string& network_id) {
  if (!system_tray_client_)
    return;
  const NetworkState* network = GetNetworkStateForGuid(network_id);
  if (!network)
    return;
  std::string error = network->GetError();
  if (!error.empty()) {
    NET_LOG(ERROR) << "Notify ShowNetworkSettings: " << NetworkId(network)
                   << ": Error: " << error;
  }
  if (!NetworkTypePattern::Primitive(network->type())
           .MatchesPattern(NetworkTypePattern::Mobile()) &&
      IsConfigurationError(error)) {
    system_tray_client_->ShowNetworkConfigure(network_id);
  } else {
    system_tray_client_->ShowNetworkSettings(network_id);
  }
}

void NetworkStateNotifier::ShowSimUnlockSettings() {
  if (!system_tray_client_)
    return;

  NET_LOG(USER) << "Opening SIM unlock settings";
  system_tray_client_->ShowSettingsSimUnlock();
}

void NetworkStateNotifier::ShowMobileDataSubpage() {
  if (!system_tray_client_) {
    return;
  }

  NET_LOG(USER) << "Opening Mobile data subpage";
  system_tray_client_->ShowMobileDataSubpage();
}

void NetworkStateNotifier::ShowApnSettings(const std::string& network_id) {
  CHECK(features::IsApnRevampEnabled());
  if (!system_tray_client_) {
    return;
  }

  NET_LOG(USER) << "Opening APN subpage for network: " << network_id;
  system_tray_client_->ShowApnSubpage(network_id);
}

void NetworkStateNotifier::ShowCarrierAccountDetail(
    const std::string& network_id) {
  NetworkConnect::Get()->ShowCarrierAccountDetail(network_id);
}

}  // namespace ash
