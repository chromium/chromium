// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/presence/conversions/nearby_presence_conversions.h"
#include "chromeos/ash/components/nearby/presence/conversions/proto_conversions.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "crypto/random.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_change_notifier.h"

namespace {

const char kFastAdvertisementServiceUuid[] =
    "0000fef3-0000-1000-8000-00805f9b34fb";
const nearby::connections::mojom::Strategy kStrategy =
    nearby::connections::mojom::Strategy::kP2pPointToPoint;

// Timeout for initiating a connection to a remote device.
constexpr base::TimeDelta kInitiateNearbyConnectionTimeout = base::Seconds(60);

// Whether or not WifiLan is supported for advertising. Support as
// a bandwidth upgrade medium is behind a feature flag.
constexpr bool kIsWifiLanAdvertisingSupported = false;

bool ShouldUseInternet(NearbyConnectionsManager::DataUsage data_usage,
                       NearbyConnectionsManager::PowerLevel power_level) {
  // We won't use internet if the user requested we don't.
  if (data_usage == NearbyConnectionsManager::DataUsage::kOffline) {
    return false;
  }

  // We won't use internet in a low power mode.
  if (power_level == NearbyConnectionsManager::PowerLevel::kLowPower) {
    return false;
  }

  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();

  // Verify that this network has an internet connection.
  if (connection_type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": No internet connection.";
    return false;
  }

  // If the user wants to limit Wi-Fi, then don't use it on metered networks.
  if (data_usage == NearbyConnectionsManager::DataUsage::kWifiOnly &&
      net::NetworkChangeNotifier::GetConnectionCost() ==
          net::NetworkChangeNotifier::CONNECTION_COST_METERED) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": Do not use internet with " << data_usage
        << " and a metered connection.";
    return false;
  }

  // We're online, the user hasn't disabled Wi-Fi, let's use it!
  return true;
}

bool ShouldEnableWebRtc(NearbyConnectionsManager::DataUsage data_usage,
                        NearbyConnectionsManager::PowerLevel power_level) {
  return base::FeatureList::IsEnabled(features::kNearbySharingWebRtc) &&
         ShouldUseInternet(data_usage, power_level);
}

bool ShouldEnableWifiLan(NearbyConnectionsManager::DataUsage data_usage,
                         NearbyConnectionsManager::PowerLevel power_level) {
  if (!base::FeatureList::IsEnabled(features::kNearbySharingWifiLan)) {
    return false;
  }

  // WifiLan only works if both devices are using the same router. We can't
  // guarantee this, but at least check that we are using Wi-Fi or ethernet.
  // TODO(https://crbug.com/1261238): Test if WifiLan can work if both devices
  // are connected to the router without an internet connection. If so, return
  // true if connection_type == net::NetworkChangeNotifier::CONNECTION_NONE.
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  bool is_connection_wifi_or_ethernet =
      connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI ||
      connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET;

  return ShouldUseInternet(data_usage, power_level) &&
         is_connection_wifi_or_ethernet;
}

std::string MediumSelectionToString(
    const nearby::connections::mojom::MediumSelection& mediums) {
  std::stringstream ss;
  ss << "{";
  if (mediums.bluetooth) {
    ss << "bluetooth ";
  }
  if (mediums.ble) {
    ss << "ble ";
  }
  if (mediums.web_rtc) {
    ss << "webrtc ";
  }
  if (mediums.wifi_lan) {
    ss << "wifilan ";
  }
  if (mediums.wifi_direct) {
    ss << "wifidirect ";
  }
  ss << "}";

  return ss.str();
}

}  // namespace

NearbyConnectionsManagerImpl::NearbyConnectionsManagerImpl(
    ash::nearby::NearbyProcessManager* process_manager,
    const std::string& service_id)
    : process_manager_(process_manager), service_id_(service_id) {
  DCHECK(process_manager_);
}

NearbyConnectionsManagerImpl::~NearbyConnectionsManagerImpl() {
  ClearIncomingPayloads();
}

void NearbyConnectionsManagerImpl::Shutdown() {
  Reset();
}

void NearbyConnectionsManagerImpl::StartAdvertising(
    std::vector<uint8_t> endpoint_info,
    IncomingConnectionListener* listener,
    NearbyConnectionsManager::PowerLevel power_level,
    NearbyConnectionsManager::DataUsage data_usage,
    ConnectionsCallback callback) {
  DCHECK(listener);
  DCHECK(!incoming_connection_listener_);

  raw_ptr<nearby::connections::mojom::NearbyConnections> nearby_connections =
      GetNearbyConnections();
  if (!nearby_connections) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  bool is_high_power =
      power_level == NearbyConnectionsManager::PowerLevel::kHighPower;
  bool use_ble = features::IsNearbyBleV2Enabled() || !is_high_power;
  auto allowed_mediums = MediumSelection::New(
      /*bluetooth=*/is_high_power, /*ble=*/use_ble,
      // Using kHighPower here rather than power_level to signal that power
      // level isn't a factor when deciding whether or not to allow WebRTC
      // upgrades from this advertisement.
      ShouldEnableWebRtc(data_usage,
                         NearbyConnectionsManager::PowerLevel::kHighPower),
      /*wifi_lan=*/
      ShouldEnableWifiLan(data_usage,
                          NearbyConnectionsManager::PowerLevel::kHighPower) &&
          kIsWifiLanAdvertisingSupported,
      /*wifi_direct=*/
      base::FeatureList::IsEnabled(features::kNearbySharingWifiDirect));
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": " << "is_high_power=" << (is_high_power ? "yes" : "no")
      << "use_ble=" << (use_ble ? "yes" : "no") << ", data_usage=" << data_usage
      << ", allowed_mediums=" << MediumSelectionToString(*allowed_mediums);

  mojo::PendingRemote<ConnectionLifecycleListener> lifecycle_listener;
  connection_lifecycle_listeners_.Add(
      this, lifecycle_listener.InitWithNewPipeAndPassReceiver());

  // Only auto-upgrade bandwidth if advertising at high-visibility.
  // This acts as a privacy safeguard when advertising in the background.
  // Bandwidth upgrades may expose stable identifiers, and so they're
  // only safe to expose after we've verified the sender's identity.
  // Once we have verified their identity, we will manually trigger
  // a bandwidth upgrade. This isn't a concern in the foreground
  // because high-visibility already leaks the device name.
  bool auto_upgrade_bandwidth = is_high_power;

  // We pass in the Fast Advertisement service UUID when we want Nearby
  // Connections to advertise a Fast Advertisement (AKA low-power, contacts
  // mode advertising). For BLE v1, this is always the case. When BLE v2 is
  // enabled, we only pass in the UUID when in low-power mode.
  // TODO (b/327451380): Replace this with a bool in the NC layer.
  std::optional<::device::BluetoothUUID> fast_advertisement_service_uuid =
      features::IsNearbyBleV2Enabled() && is_high_power
          ? std::nullopt
          : std::make_optional(
                device::BluetoothUUID(kFastAdvertisementServiceUuid));

  incoming_connection_listener_ = listener;
  nearby_connections->StartAdvertising(
      service_id_, endpoint_info,
      AdvertisingOptions::New(kStrategy, std::move(allowed_mediums),
                              auto_upgrade_bandwidth,
                              /*enforce_topology_constraints=*/true,
                              /*enable_bluetooth_listening=*/use_ble,
                              /*enable_webrtc_listening=*/
                              ShouldEnableWebRtc(data_usage, power_level),
                              /*fast_advertisement_service_uuid=*/
                              fast_advertisement_service_uuid),
      std::move(lifecycle_listener), std::move(callback));
}

void NearbyConnectionsManagerImpl::StopAdvertising(
    ConnectionsCallback callback) {
  incoming_connection_listener_ = nullptr;

  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  process_reference_->GetNearbyConnections()->StopAdvertising(
      service_id_, std::move(callback));
}

void NearbyConnectionsManagerImpl::InjectBluetoothEndpoint(
    const std::string& service_id,
    const std::string& endpoint_id,
    const std::vector<uint8_t> endpoint_info,
    const std::vector<uint8_t> remote_bluetooth_mac_address,
    ConnectionsCallback callback) {
  nearby::connections::mojom::NearbyConnections* nearby_connections =
      GetNearbyConnections();
  if (!nearby_connections) {
    CD_LOG(ERROR, Feature::NS)
        << __func__ << " Nearby Connections cannot be retrieved.";
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  if (endpoint_id.length() != 4) {
    CD_LOG(ERROR, Feature::NS)
        << __func__ << " endpoint ID must be length 4. Actual size: "
        << base::NumberToString(endpoint_id.length());
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  if (endpoint_info.size() == 0 || endpoint_info.size() > 130) {
    CD_LOG(ERROR, Feature::NS)
        << __func__
        << " endpoint info must have size >0 and <131. Actual size: "
        << base::NumberToString(endpoint_info.size());
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  if (remote_bluetooth_mac_address.size() != 6) {
    CD_LOG(ERROR, Feature::NS)
        << __func__
        << " bluetooth mac address size must be 6 bytes. Actual size: "
        << base::NumberToString(remote_bluetooth_mac_address.size());
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  nearby_connections->InjectBluetoothEndpoint(
      service_id, endpoint_id, endpoint_info, remote_bluetooth_mac_address,
      std::move(callback));
}

void NearbyConnectionsManagerImpl::StartDiscovery(
    DiscoveryListener* listener,
    NearbyConnectionsManager::DataUsage data_usage,
    ConnectionsCallback callback) {
  DCHECK(listener);
  DCHECK(!discovery_listener_);

  raw_ptr<nearby::connections::mojom::NearbyConnections> nearby_connections =
      GetNearbyConnections();
  if (!nearby_connections) {
    std::move(callback).Run(ConnectionsStatus::kError);
    return;
  }

  auto allowed_mediums = MediumSelection::New(
      /*bluetooth=*/true,
      /*ble=*/true,
      /*webrtc=*/
      ShouldEnableWebRtc(data_usage,
                         NearbyConnectionsManager::PowerLevel::kHighPower),
      /*wifi_lan=*/
      ShouldEnableWifiLan(data_usage, PowerLevel::kHighPower) &&
          ::features::IsNearbyMdnsEnabled(),
      /*wifi_direct=*/
      base::FeatureList::IsEnabled(features::kNearbySharingWifiDirect));
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": " << "data_usage=" << data_usage
      << ", allowed_mediums=" << MediumSelectionToString(*allowed_mediums);

  discovery_listener_ = listener;
  nearby_connections->StartDiscovery(
      service_id_,
      DiscoveryOptions::New(
          kStrategy, std::move(allowed_mediums),
          device::BluetoothUUID(kFastAdvertisementServiceUuid),
          /*is_out_of_band_connection=*/false),
      endpoint_discovery_listener_.BindNewPipeAndPassRemote(),
      std::move(callback));
}

void NearbyConnectionsManagerImpl::StopDiscovery() {
  discovered_endpoints_.clear();
  discovery_listener_ = nullptr;
  endpoint_discovery_listener_.reset();

  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  process_reference_->GetNearbyConnections()->StopDiscovery(
      service_id_, base::BindOnce([](ConnectionsStatus status) {
        CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
            << __func__
            << ": Stop discovery attempted over Nearby "
               "Connections with result: "
            << ConnectionsStatusToString(status);
      }));
}

void NearbyConnectionsManagerImpl::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    std::optional<std::vector<uint8_t>> bluetooth_mac_address,
    NearbyConnectionsManager::DataUsage data_usage,
    NearbyConnectionCallback callback) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (bluetooth_mac_address && bluetooth_mac_address->size() != 6) {
    bluetooth_mac_address.reset();
  }

  auto allowed_mediums = MediumSelection::New(
      /*bluetooth=*/true,
      /*ble=*/false,
      /*web_rtc=*/ShouldEnableWebRtc(data_usage, PowerLevel::kHighPower),
      /*wifi_lan=*/ShouldEnableWifiLan(data_usage, PowerLevel::kHighPower),
      /*wifi_direct=*/
      base::FeatureList::IsEnabled(features::kNearbySharingWifiDirect));
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": " << "data_usage=" << data_usage
      << ", allowed_mediums=" << MediumSelectionToString(*allowed_mediums);

  mojo::PendingRemote<ConnectionLifecycleListener> lifecycle_listener;
  connection_lifecycle_listeners_.Add(
      this, lifecycle_listener.InitWithNewPipeAndPassReceiver());

  auto result =
      pending_outgoing_connections_.emplace(endpoint_id, std::move(callback));
  DCHECK(result.second);

  auto timeout_timer = std::make_unique<base::OneShotTimer>();
  timeout_timer->Start(
      FROM_HERE, kInitiateNearbyConnectionTimeout,
      base::BindOnce(&NearbyConnectionsManagerImpl::OnConnectionTimedOut,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id));
  connect_timeout_timers_.emplace(endpoint_id, std::move(timeout_timer));

  process_reference_->GetNearbyConnections()->RequestConnection(
      service_id_, endpoint_info, endpoint_id,
      ConnectionOptions::New(std::move(allowed_mediums),
                             std::move(bluetooth_mac_address),
                             /*keep_alive_interval_millis=*/std::nullopt,
                             /*keep_alive_timeout_millis=*/std::nullopt),
      std::move(lifecycle_listener),
      base::BindOnce(&NearbyConnectionsManagerImpl::OnConnectionRequested,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id));
}

void NearbyConnectionsManagerImpl::OnConnectionTimedOut(
    const std::string& endpoint_id) {
  CD_LOG(ERROR, Feature::NEARBY_INFRA)
      << "Failed to connect to the remote shareTarget: Timed out.";
  Disconnect(endpoint_id);
}

void NearbyConnectionsManagerImpl::OnConnectionTimedOutV3(
    const std::string& endpoint_id) {
  if (base::Contains(endpoint_id_to_presence_device_map_, endpoint_id)) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__
        << "(V3) Failed to connect to the remote shareTarget: Timed out.";
    DisconnectV3(*endpoint_id_to_presence_device_map_.at(endpoint_id).get());
  } else {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << __func__ << "Timed out, but no endpoint_id in PresenceDevice map.";
  }
}

void NearbyConnectionsManagerImpl::OnConnectionRequested(
    const std::string& endpoint_id,
    ConnectionsStatus status) {
  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it == pending_outgoing_connections_.end()) {
    return;
  }

  if (status != ConnectionsStatus::kSuccess) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "Failed to connect to the remote shareTarget: "
        << ConnectionsStatusToString(status);
    Disconnect(endpoint_id);
    return;
  }

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnConnectionRequestedV3(
    nearby::presence::PresenceDevice remote_device,
    ConnectionsStatus status) {
  if (status != ConnectionsStatus::kSuccess) {
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << "Failed to connect (v3) to remote device with result: "
        << ConnectionsStatusToString(status);
    DisconnectV3(remote_device);
    return;
  }
}

void NearbyConnectionsManagerImpl::Disconnect(const std::string& endpoint_id) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  process_reference_->GetNearbyConnections()->DisconnectFromEndpoint(
      service_id_, endpoint_id,
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
                << __func__ << ": Disconnecting from endpoint " << endpoint_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          endpoint_id));

  OnDisconnected(endpoint_id);
  CD_LOG(INFO, Feature::NEARBY_INFRA) << "Disconnected from " << endpoint_id;
}

void NearbyConnectionsManagerImpl::Send(
    const std::string& endpoint_id,
    PayloadPtr payload,
    base::WeakPtr<PayloadStatusListener> listener) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  if (listener) {
    RegisterPayloadStatusListener(payload->id, listener);
  }

  process_reference_->GetNearbyConnections()->SendPayload(
      service_id_, {endpoint_id}, std::move(payload),
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
                << __func__ << ": Sending payload to endpoint " << endpoint_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          endpoint_id));
}

void NearbyConnectionsManagerImpl::RegisterPayloadStatusListener(
    int64_t payload_id,
    base::WeakPtr<PayloadStatusListener> listener) {
  payload_status_listeners_.insert_or_assign(payload_id, listener);
}

void NearbyConnectionsManagerImpl::RegisterPayloadPath(
    int64_t payload_id,
    const base::FilePath& file_path,
    ConnectionsCallback callback) {
  if (!process_reference_) {
    return;
  }

  DCHECK(!file_path.empty());

  file_handler_.CreateFile(
      file_path, base::BindOnce(&NearbyConnectionsManagerImpl::OnFileCreated,
                                weak_ptr_factory_.GetWeakPtr(), payload_id,
                                std::move(callback)));
}

void NearbyConnectionsManagerImpl::OnFileCreated(
    int64_t payload_id,
    ConnectionsCallback callback,
    NearbyFileHandler::CreateFileResult result) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  process_reference_->GetNearbyConnections()->RegisterPayloadFile(
      service_id_, payload_id, std::move(result.input_file),
      std::move(result.output_file), std::move(callback));
}

NearbyConnectionsManagerImpl::Payload*
NearbyConnectionsManagerImpl::GetIncomingPayload(int64_t payload_id) {
  auto it = incoming_payloads_.find(payload_id);
  if (it == incoming_payloads_.end()) {
    return nullptr;
  }

  return it->second.get();
}

void NearbyConnectionsManagerImpl::Cancel(int64_t payload_id) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  auto it = payload_status_listeners_.find(payload_id);
  if (it != payload_status_listeners_.end()) {
    base::WeakPtr<PayloadStatusListener> listener = it->second;
    payload_status_listeners_.erase(payload_id);

    // Note: The listener might be invalidated, for example, if it is shared
    // with another payload in the same transfer.
    if (listener) {
      listener->OnStatusUpdate(
          PayloadTransferUpdate::New(payload_id, PayloadStatus::kCanceled,
                                     /*total_bytes=*/0,
                                     /*bytes_transferred=*/0),
          /*upgraded_medium=*/std::nullopt);
    }
  }

  process_reference_->GetNearbyConnections()->CancelPayload(
      service_id_, payload_id,
      base::BindOnce(
          [](int64_t payload_id, ConnectionsStatus status) {
            CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
                << __func__ << ": Cancelling payload to id " << payload_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          payload_id));
  CD_LOG(INFO, Feature::NEARBY_INFRA) << "Cancelling payload: " << payload_id;
}

void NearbyConnectionsManagerImpl::ClearIncomingPayloads() {
  std::vector<PayloadPtr> payloads;
  for (auto& it : incoming_payloads_) {
    payloads.push_back(std::move(it.second));
    payload_status_listeners_.erase(it.first);
  }

  file_handler_.ReleaseFilePayloads(std::move(payloads));
  incoming_payloads_.clear();
}

void NearbyConnectionsManagerImpl::ClearIncomingPayloadWithId(
    int64_t payload_id) {
  auto payload_found = incoming_payloads_.find(payload_id);
  if (payload_found != incoming_payloads_.end()) {
    std::vector<PayloadPtr> payload_found_container;
    payload_found_container.push_back(std::move(payload_found->second));
    file_handler_.ReleaseFilePayloads(std::move(payload_found_container));
    incoming_payloads_.erase(payload_found);
  }
  payload_status_listeners_.erase(payload_id);
}

std::optional<std::string> NearbyConnectionsManagerImpl::GetAuthenticationToken(
    const std::string& endpoint_id) {
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end()) {
    return std::nullopt;
  }

  return it->second->authentication_token;
}

std::optional<std::vector<uint8_t>>
NearbyConnectionsManagerImpl::GetRawAuthenticationToken(
    const std::string& endpoint_id) {
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end()) {
    return std::nullopt;
  }

  return it->second->raw_authentication_token;
}

void NearbyConnectionsManagerImpl::RegisterBandwidthUpgradeListener(
    base::WeakPtr<BandwidthUpgradeListener> listener) {
  CHECK(!bandwidth_upgrade_listener_);
  bandwidth_upgrade_listener_ = listener;
}

void NearbyConnectionsManagerImpl::UpgradeBandwidth(
    const std::string& endpoint_id) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  // The only bandwidth upgrade mediums at this point are WebRTC, WifiLan, and
  // WifiDirect.
  if (!base::FeatureList::IsEnabled(features::kNearbySharingWebRtc) &&
      !base::FeatureList::IsEnabled(features::kNearbySharingWifiLan) &&
      !base::FeatureList::IsEnabled(features::kNearbySharingWifiDirect)) {
    return;
  }

  requested_bwu_endpoint_ids_.emplace(endpoint_id);
  process_reference_->GetNearbyConnections()->InitiateBandwidthUpgrade(
      service_id_, endpoint_id,
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
                << __func__ << ": Bandwidth upgrade attempted to endpoint "
                << endpoint_id << "over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
            base::UmaHistogramBoolean(
                "Nearby.Share.Medium.InitiateBandwidthUpgradeResult",
                status == ConnectionsStatus::kSuccess);
          },
          endpoint_id));
}

void NearbyConnectionsManagerImpl::ConnectV3(
    nearby::presence::PresenceDevice remote_presence_device,
    NearbyConnectionsManager::DataUsage data_usage,
    NearbyConnectionCallback callback) {
  raw_ptr<nearby::connections::mojom::NearbyConnections> nearby_connections =
      GetNearbyConnections();
  CHECK(nearby_connections);

  const std::string& endpoint_id = remote_presence_device.GetEndpointId();
  endpoint_id_to_presence_device_map_.emplace(
      endpoint_id, std::make_unique<nearby::presence::PresenceDevice>(
                       std::move(remote_presence_device)));

  // TODO(b/287340241): Enable BLE connections as an allowed medium.
  auto allowed_mediums = MediumSelection::New(
      /*bluetooth=*/true,
      /*ble=*/false, ShouldEnableWebRtc(data_usage, PowerLevel::kHighPower),
      /*wifi_lan=*/ShouldEnableWifiLan(data_usage, PowerLevel::kHighPower),
      /*wifi_direct=*/
      base::FeatureList::IsEnabled(features::kNearbySharingWifiDirect));
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": " << "data_usage=" << data_usage
      << ", allowed_mediums=" << MediumSelectionToString(*allowed_mediums);

  mojo::PendingRemote<ConnectionListenerV3> connection_listener_v3;
  connection_listener_v3s_.Add(
      this, connection_listener_v3.InitWithNewPipeAndPassReceiver());

  pending_outgoing_connections_.emplace(endpoint_id, std::move(callback));

  auto timeout_timer = std::make_unique<base::OneShotTimer>();
  timeout_timer->Start(
      FROM_HERE, kInitiateNearbyConnectionTimeout,
      base::BindOnce(&NearbyConnectionsManagerImpl::OnConnectionTimedOutV3,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id));
  connect_timeout_timers_v3_.emplace(endpoint_id, std::move(timeout_timer));

  endpoint_id_to_connect_v3_start_time_.emplace(endpoint_id,
                                                base::TimeTicks::Now());

  auto presence_device =
      *endpoint_id_to_presence_device_map_.at(endpoint_id).get();

  nearby_connections->RequestConnectionV3(
      service_id_,
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device),
      ConnectionOptions::New(std::move(allowed_mediums),
                             /*bluetooth_mac_address=*/std::nullopt,
                             /*keep_alive_interval_millis=*/std::nullopt,
                             /*keep_alive_timeout_millis=*/std::nullopt),
      std::move(connection_listener_v3),
      base::BindOnce(&NearbyConnectionsManagerImpl::OnConnectionRequestedV3,
                     weak_ptr_factory_.GetWeakPtr(), presence_device));
}

void NearbyConnectionsManagerImpl::DisconnectV3(
    nearby::presence::PresenceDevice remote_presence_device) {
  if (!process_reference_) {
    return;
  }

  process_reference_->GetNearbyConnections()->DisconnectFromDeviceV3(
      service_id_,
      ash::nearby::presence::BuildPresenceMojomDevice(remote_presence_device),
      base::BindOnce([](ConnectionsStatus status) {
        CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
            << __func__ << ": Disconnect (V3) from device "
            << "attempted over Nearby Connections with result: "
            << ConnectionsStatusToString(status);
      }));

  // `OnDisconnectedV3()` is called because if the remote_presence_device hasn't
  // been connected yet, ConnectionListenerV3 is not notified of the
  // disconnection event. Directly calling here will ensure the cleanup.
  OnDisconnectedV3(remote_presence_device.GetEndpointId());
}

base::WeakPtr<NearbyConnectionsManager>
NearbyConnectionsManagerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void NearbyConnectionsManagerImpl::OnNearbyProcessStopped(
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason) {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA) << __func__;
  process_reference_.reset();
  Reset();
}

void NearbyConnectionsManagerImpl::OnEndpointFound(
    const std::string& endpoint_id,
    DiscoveredEndpointInfoPtr info) {
  if (!discovery_listener_) {
    CD_LOG(INFO, Feature::NEARBY_INFRA) << "Ignoring discovered endpoint "
                                        << base::HexEncode(info->endpoint_info)
                                        << " because we're no longer "
                                           "in discovery mode";
    return;
  }

  auto result = discovered_endpoints_.insert(endpoint_id);
  if (!result.second) {
    CD_LOG(INFO, Feature::NEARBY_INFRA) << "Ignoring discovered endpoint "
                                        << base::HexEncode(info->endpoint_info)
                                        << " because we've already "
                                           "reported this endpoint";
    return;
  }

  discovery_listener_->OnEndpointDiscovered(endpoint_id, info->endpoint_info);
  CD_LOG(INFO, Feature::NEARBY_INFRA)
      << "Discovered " << base::HexEncode(info->endpoint_info)
      << " over Nearby Connections";
}

void NearbyConnectionsManagerImpl::OnEndpointLost(
    const std::string& endpoint_id) {
  if (!discovered_endpoints_.erase(endpoint_id)) {
    CD_LOG(INFO, Feature::NEARBY_INFRA)
        << "Ignoring lost endpoint " << endpoint_id
        << " because we haven't reported this endpoint";
    return;
  }

  if (!discovery_listener_) {
    CD_LOG(INFO, Feature::NEARBY_INFRA)
        << "Ignoring lost endpoint " << endpoint_id
        << " because we're no longer in discovery mode";
    return;
  }

  discovery_listener_->OnEndpointLost(endpoint_id);
  CD_LOG(INFO, Feature::NEARBY_INFRA)
      << "Endpoint " << endpoint_id << " lost over Nearby Connections";
}

void NearbyConnectionsManagerImpl::OnConnectionInitiated(
    const std::string& endpoint_id,
    ConnectionInfoPtr info) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  bool is_incoming_connection = info->is_incoming_connection;
  const std::vector<uint8_t>& endpoint_info = info->endpoint_info;
  auto result = connection_info_map_.emplace(endpoint_id, std::move(info));
  DCHECK(result.second);

  mojo::PendingRemote<PayloadListener> payload_listener;
  payload_listeners_.Add(this,
                         payload_listener.InitWithNewPipeAndPassReceiver());

  if (is_incoming_connection && incoming_connection_listener_) {
    incoming_connection_listener_->OnIncomingConnectionInitiated(endpoint_id,
                                                                 endpoint_info);
  }

  process_reference_->GetNearbyConnections()->AcceptConnection(
      service_id_, endpoint_id, std::move(payload_listener),
      base::BindOnce(
          [](const std::string& endpoint_id, ConnectionsStatus status) {
            CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
                << __func__ << ": Accept connection attempted to endpoint "
                << endpoint_id << " over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
          },
          endpoint_id));
}

void NearbyConnectionsManagerImpl::OnConnectionAccepted(
    const std::string& endpoint_id) {
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end()) {
    return;
  }

  if (it->second->is_incoming_connection) {
    if (!incoming_connection_listener_) {
      // Not in advertising mode.
      Disconnect(endpoint_id);
      return;
    }

    auto result = connections_.emplace(
        endpoint_id, std::make_unique<NearbyConnectionImpl>(
                         weak_ptr_factory_.GetWeakPtr(), endpoint_id));
    DCHECK(result.second);
    incoming_connection_listener_->OnIncomingConnectionAccepted(
        endpoint_id, it->second->endpoint_info, result.first->second.get());
  } else {
    auto pending_it = pending_outgoing_connections_.find(endpoint_id);
    if (pending_it == pending_outgoing_connections_.end()) {
      Disconnect(endpoint_id);
      return;
    }

    auto result = connections_.emplace(
        endpoint_id, std::make_unique<NearbyConnectionImpl>(
                         weak_ptr_factory_.GetWeakPtr(), endpoint_id));
    DCHECK(result.second);
    std::move(pending_it->second)
        .Run(
            /*nearby_connection=*/result.first->second.get());
    pending_outgoing_connections_.erase(pending_it);
    connect_timeout_timers_.erase(endpoint_id);
  }
}

void NearbyConnectionsManagerImpl::OnConnectionRejected(
    const std::string& endpoint_id,
    Status status) {
  connection_info_map_.erase(endpoint_id);

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it != pending_outgoing_connections_.end()) {
    std::move(it->second).Run(/*nearby_connection=*/nullptr);
    pending_outgoing_connections_.erase(it);
    connect_timeout_timers_.erase(endpoint_id);
  }

  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnDisconnected(
    const std::string& endpoint_id) {
  connection_info_map_.erase(endpoint_id);

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it != pending_outgoing_connections_.end()) {
    std::move(it->second).Run(nullptr);
    pending_outgoing_connections_.erase(it);
    connect_timeout_timers_.erase(endpoint_id);
  }

  // TODO(b/326139109): Refactor to add a check for `endpoint_id` to ensure that
  // it exists in either `pending_outgoing_connections_` or `connections_`.
  // Otherwise we should `ReportBadMessage()`.

  // Destroying the NearbyConnectionImpl object may start a chain of callbacks
  // that can delete this NearbyConnectionsManagerImpl object. This may result
  // in a crash (see b/303675257). Update the |connections_| map, but wait to
  // destroy the connection object until after we're done modifying internal
  // state in OnDisconnected() by letting the connection go out of scope.
  std::unique_ptr<NearbyConnectionImpl> connection =
      std::move(connections_[endpoint_id]);
  connections_.erase(endpoint_id);

  if (base::Contains(requested_bwu_endpoint_ids_, endpoint_id)) {
    base::UmaHistogramBoolean(
        "Nearby.Share.Medium.RequestedBandwidthUpgradeResult",
        base::Contains(current_upgraded_mediums_, endpoint_id));
  }
  requested_bwu_endpoint_ids_.erase(endpoint_id);
  on_bandwidth_changed_endpoint_ids_.erase(endpoint_id);
  current_upgraded_mediums_.erase(endpoint_id);
}

void NearbyConnectionsManagerImpl::OnBandwidthChanged(
    const std::string& endpoint_id,
    Medium medium) {
  // `OnBandwidthChanged` is always called for the first Medium we connected to.
  // This is not guaranteed to be a specific Medium, but is usually Bluetooth.
  // This may or may not be preceded by a call to `UpgradeBandwidth`. It's not
  // useful to record this first Medium since no Bandwidth Upgrade occurred, so
  // we ignore it.
  if (!base::Contains(on_bandwidth_changed_endpoint_ids_, endpoint_id)) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": Initial call with medium=" << medium
        << "; endpoint_id=" << endpoint_id;
    if (bandwidth_upgrade_listener_) {
      bandwidth_upgrade_listener_->OnInitialMedium(endpoint_id, medium);
    }
    on_bandwidth_changed_endpoint_ids_.emplace(endpoint_id);
  } else {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": Changed to medium=" << medium
        << "; endpoint_id=" << endpoint_id;
    base::UmaHistogramEnumeration("Nearby.Share.Medium.ChangedToMedium",
                                  medium);
    current_upgraded_mediums_.insert_or_assign(endpoint_id, medium);
    // Only propagate this event on actual bandwidth upgrades.
    if (bandwidth_upgrade_listener_) {
      bandwidth_upgrade_listener_->OnBandwidthUpgrade(endpoint_id, medium);
    }
  }
  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnPayloadReceived(
    const std::string& endpoint_id,
    PayloadPtr payload) {
  auto result = incoming_payloads_.emplace(payload->id, std::move(payload));
  DCHECK(result.second);
}

void NearbyConnectionsManagerImpl::OnPayloadTransferUpdate(
    const std::string& endpoint_id,
    PayloadTransferUpdatePtr update) {
  // TODO(https://crbug.com/1177088): Determine if we should attempt to bind to
  // process.
  if (!process_reference_) {
    return;
  }

  // If this is a payload we've registered for, then forward its status to the
  // PayloadStatusListener if it still exists. We don't need to do anything more
  // with the payload.
  auto listener_it = payload_status_listeners_.find(update->payload_id);
  if (listener_it != payload_status_listeners_.end()) {
    base::WeakPtr<PayloadStatusListener> listener = listener_it->second;
    switch (update->status) {
      case PayloadStatus::kInProgress:
        break;
      case PayloadStatus::kSuccess:
      case PayloadStatus::kCanceled:
      case PayloadStatus::kFailure:
        payload_status_listeners_.erase(update->payload_id);
        break;
    }
    // Note: The listener might be invalidated, for example, if it is shared
    // with another payload in the same transfer.
    if (listener) {
      listener->OnStatusUpdate(std::move(update),
                               GetUpgradedMedium(endpoint_id));
    }
    return;
  }

  // If this is an incoming payload that we have not registered for, then we'll
  // treat it as a control frame (eg. IntroductionFrame) and forward it to the
  // associated NearbyConnection.
  auto payload_it = incoming_payloads_.find(update->payload_id);
  if (payload_it == incoming_payloads_.end()) {
    return;
  }

  if (!payload_it->second->content->is_bytes()) {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << "Received unknown payload of file type. Cancelling.";
    process_reference_->GetNearbyConnections()->CancelPayload(
        service_id_, payload_it->first, base::DoNothing());
    return;
  }

  if (update->status != PayloadStatus::kSuccess) {
    return;
  }

  auto connections_it = connections_.find(endpoint_id);
  if (connections_it == connections_.end()) {
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << "Writing incoming byte message to NearbyConnection.";
  connections_it->second->WriteMessage(
      payload_it->second->content->get_bytes()->bytes);
}

void NearbyConnectionsManagerImpl::OnConnectionInitiatedV3(
    const std::string& endpoint_id,
    InitialConnectionInfoV3Ptr info) {
  if (!process_reference_) {
    return;
  }

  if (!base::Contains(endpoint_id_to_presence_device_map_, endpoint_id)) {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << __func__ << "Received endpoint_id for device no longer in map.";
    return;
  }

  auto presence_device =
      *endpoint_id_to_presence_device_map_.at(endpoint_id).get();

  if (info->authentication_status ==
      nearby::connections::mojom::AuthenticationStatus::kSuccess) {
    mojo::PendingRemote<PayloadListenerV3> payload_listener;
    payload_listener_v3s_.Add(
        this, payload_listener.InitWithNewPipeAndPassReceiver());

    process_reference_->GetNearbyConnections()->AcceptConnectionV3(
        service_id_,
        ash::nearby::presence::BuildPresenceMojomDevice(presence_device),
        std::move(payload_listener),
        base::BindOnce([](ConnectionsStatus status) {
          CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
              << __func__ << ": Accept connection (V3) attempted to device "
              << " over Nearby Connections with result: "
              << ConnectionsStatusToString(status);
        }));
  } else {
    process_reference_->GetNearbyConnections()->RejectConnectionV3(
        service_id_,
        ash::nearby::presence::BuildPresenceMojomDevice(presence_device),
        base::BindOnce([](ConnectionsStatus status) {
          CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
              << __func__ << ": Reject connection (V3) attempted to device "
              << " over Nearby Connections with result: "
              << ConnectionsStatusToString(status);
        }));
  }
}

void NearbyConnectionsManagerImpl::OnConnectionResultV3(
    const std::string& endpoint_id,
    Status status) {
  CD_LOG(INFO, Feature::NEARBY_INFRA) << __func__ << ": result=" << status;

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it == pending_outgoing_connections_.end() ||
      !base::Contains(endpoint_id_to_connect_v3_start_time_, endpoint_id)) {
    connection_listener_v3s_.ReportBadMessage(base::StringPrintf(
        "OnConnectionResultV3() received endpoint_id=%s which "
        "does not exist in connections V3",
        endpoint_id.c_str()));
    return;
  }

  if (status == Status::kSuccess) {
    auto result = connections_v3_.emplace(
        endpoint_id, std::make_unique<NearbyConnectionImpl>(
                         weak_ptr_factory_.GetWeakPtr(), endpoint_id));
    std::move(it->second)
        .Run(
            /*nearby_connection=*/result.first->second.get());

    base::UmaHistogramTimes(
        "Nearby.Connections.V3.ConnectionResult.Success.Latency",
        base::TimeTicks::Now() -
            endpoint_id_to_connect_v3_start_time_.at(endpoint_id));
  } else {
    std::move(it->second).Run(/*nearby_connection=*/nullptr);
  }

  base::UmaHistogramEnumeration("Nearby.Connections.V3.Connection.Result",
                                status);
  pending_outgoing_connections_.erase(it);
  connect_timeout_timers_v3_.erase(endpoint_id);
  endpoint_id_to_connect_v3_start_time_.erase(endpoint_id);
}

void NearbyConnectionsManagerImpl::OnDisconnectedV3(
    const std::string& endpoint_id) {
  endpoint_id_to_presence_device_map_.erase(endpoint_id);

  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it != pending_outgoing_connections_.end()) {
    std::move(it->second).Run(/*nearby_connection=*/nullptr);
    pending_outgoing_connections_.erase(it);
    connect_timeout_timers_v3_.erase(endpoint_id);
  } else {
    // Destroying the NearbyConnectionImpl object may start a chain of callbacks
    // that can delete this NearbyConnectionsManagerImpl object. This may result
    // in a crash (see b/303675257). Update the |connections_v3_| map, but wait
    // to destroy the connection object until after we're done modifying
    // internal state in OnDisconnected() by letting the connection go out of
    // scope.
    std::unique_ptr<NearbyConnectionImpl> connection;
    auto active_connections_it = connections_v3_.find(endpoint_id);

    // The specified endpoint_id was not in pending connections, and thus must
    // be in active `connections_v3_`. If not, report a bad message from Nearby.
    if (active_connections_it == connections_v3_.end()) {
      connection_listener_v3s_.ReportBadMessage(
          base::StringPrintf("OnDisconnectedV3() received endpoint_id=%s which "
                             "does not exist in pending connections or "
                             "connections V3",
                             endpoint_id.c_str()));
    } else {
      connection = std::move(active_connections_it->second);
      connections_v3_.erase(active_connections_it);
    }
  }

  // TODO(b/325534442): Emit to V3 version of the metric
  // RequestedBandwidthUpgradeResult, and updated BWU-related maps. See older
  // OnDisconnected() method.

  on_bandwidth_changed_endpoint_ids_.erase(endpoint_id);
  current_upgraded_mediums_v3_.erase(endpoint_id);
}

void NearbyConnectionsManagerImpl::OnBandwidthChangedV3(
    const std::string& endpoint_id,
    BandwidthInfoPtr bandwidth_info) {
  // `OnBandwidthChanged` is always called for the first Medium we connected to.
  // This is not guaranteed to be a specific Medium, but is usually Bluetooth.
  // This may or may not be preceded by a call to `UpgradeBandwidth`. It's not
  // useful to record this first Medium since no Bandwidth Upgrade occurred, so
  // we ignore it.
  if (!base::Contains(on_bandwidth_changed_endpoint_ids_v3_, endpoint_id)) {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__
        << ": (V3) Initial call with medium=" << bandwidth_info->medium
        << " , quality=" << bandwidth_info->quality
        << "; endpoint_id=" << endpoint_id;
    on_bandwidth_changed_endpoint_ids_v3_.emplace(endpoint_id);
  } else {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": (V3) Changed to medium=" << bandwidth_info->medium
        << " , quality=" << bandwidth_info->quality
        << "; endpoint_id=" << endpoint_id;
    base::UmaHistogramEnumeration(
        "Nearby.Connections.V3.Medium.ChangedToMedium", bandwidth_info->medium);
    current_upgraded_mediums_v3_.insert_or_assign(endpoint_id,
                                                  bandwidth_info->medium);

    if (base::Contains(endpoint_id_to_presence_device_map_, endpoint_id) &&
        bandwidth_upgrade_listener_) {
      // TODO(b/337049943): Determine whether to pass back the entire
      // `PresenceDevice` or just the `endpoint_id`.
      bandwidth_upgrade_listener_->OnBandwidthUpgradeV3(
          *endpoint_id_to_presence_device_map_.at(endpoint_id).get(),
          bandwidth_info->medium);
    }
  }
}

void NearbyConnectionsManagerImpl::OnPayloadReceivedV3(
    const std::string& endpoint_id,
    PayloadPtr payload) {
  if (!base::Contains(endpoint_id_to_presence_device_map_, endpoint_id)) {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << __func__ << "Received endpoint_id for device not in map.";
    return;
  }

  incoming_payloads_.emplace(payload->id, std::move(payload));
}

void NearbyConnectionsManagerImpl::OnPayloadTransferUpdateV3(
    const std::string& endpoint_id,
    PayloadTransferUpdatePtr update) {
  if (!base::Contains(endpoint_id_to_presence_device_map_, endpoint_id)) {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << __func__ << "Received endpoint_id for device not in map.";
    return;
  }

  // The implementation of the current v3::PayloadStatusListener operates in the
  // same way as the V1 variant so we can leverage `OnPayloadTransferUpdate()`.
  // This may change in the future.
  OnPayloadTransferUpdate(endpoint_id, std::move(update));
}

raw_ptr<nearby::connections::mojom::NearbyConnections>
NearbyConnectionsManagerImpl::GetNearbyConnections() {
  if (!process_reference_) {
    process_reference_ = process_manager_->GetNearbyProcessReference(
        base::BindOnce(&NearbyConnectionsManagerImpl::OnNearbyProcessStopped,
                       base::Unretained(this)));

    if (!process_reference_) {
      CD_LOG(WARNING, Feature::NEARBY_INFRA)
          << __func__ << "Failed to get a reference to the nearby process.";
      return nullptr;
    }
  }

  nearby::connections::mojom::NearbyConnections* nearby_connections =
      process_reference_->GetNearbyConnections().get();

  if (!nearby_connections) {
    CD_LOG(WARNING, Feature::NEARBY_INFRA)
        << __func__
        << "Failed to get a nearby connections from process reference.";
  }

  return nearby_connections;
}

void NearbyConnectionsManagerImpl::Reset() {
  if (process_reference_) {
    process_reference_->GetNearbyConnections()->StopAllEndpoints(
        service_id_, base::BindOnce([](ConnectionsStatus status) {
          CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
              << __func__
              << ": Stop all endpoints attempted over Nearby "
                 "Connections with result: "
              << ConnectionsStatusToString(status);
        }));
  }
  process_reference_.reset();
  discovered_endpoints_.clear();
  payload_status_listeners_.clear();
  ClearIncomingPayloads();
  connections_.clear();
  connections_v3_.clear();
  connection_info_map_.clear();
  discovery_listener_ = nullptr;
  incoming_connection_listener_ = nullptr;
  endpoint_discovery_listener_.reset();
  connect_timeout_timers_.clear();
  connect_timeout_timers_v3_.clear();
  requested_bwu_endpoint_ids_.clear();
  on_bandwidth_changed_endpoint_ids_.clear();
  on_bandwidth_changed_endpoint_ids_v3_.clear();
  current_upgraded_mediums_.clear();
  current_upgraded_mediums_v3_.clear();
  endpoint_id_to_connect_v3_start_time_.clear();

  for (auto& entry : pending_outgoing_connections_) {
    std::move(entry.second).Run(/*connection=*/nullptr);
  }

  pending_outgoing_connections_.clear();
}

std::optional<nearby::connections::mojom::Medium>
NearbyConnectionsManagerImpl::GetUpgradedMedium(
    const std::string& endpoint_id) const {
  const auto it = current_upgraded_mediums_.find(endpoint_id);
  if (it != current_upgraded_mediums_.end()) {
    return it->second;
  }

  const auto it_v3 = current_upgraded_mediums_v3_.find(endpoint_id);
  if (it_v3 != current_upgraded_mediums_v3_.end()) {
    return it_v3->second;
  }

  return std::nullopt;
}
