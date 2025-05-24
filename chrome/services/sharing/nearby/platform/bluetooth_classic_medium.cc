// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_classic_medium.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_server_socket.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_socket.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace nearby::chrome {

namespace {

// Client name for logging in BLE scanning.
constexpr char kScanClientName[] = "Nearby Connections";

// Duration of time after which inactive Bluetooth devices may be removed from
// the discovered devices map.
const base::TimeDelta kStaleBluetoothDeviceTimeout = base::Seconds(20);

void LogStartDiscoveryResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.ClassicMedium.StartDiscovery.Result",
      success);
}

void LogStopDiscoveryResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.ClassicMedium.StopDiscovery.Result",
      success);
}

void LogConnectToServiceResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.ClassicMedium.ConnectToService.Result",
      success);
}

void LogConnectToServiceDuration(base::TimeDelta duration) {
  base::UmaHistogramTimes(
      "Nearby.Connections.Bluetooth.ClassicMedium.ConnectToService.Duration",
      duration);
}

void LogListenForServiceResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.ClassicMedium.ListenForService.Result",
      success);
}

}  // namespace

BluetoothClassicMedium::BluetoothClassicMedium(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter)
    : adapter_(adapter),
      stale_bluetooth_device_timer_(
          FROM_HERE,
          kStaleBluetoothDeviceTimeout / 4,
          base::BindRepeating(
              &BluetoothClassicMedium::RemoveStaleBluetoothDevices,
              base::Unretained(this))) {
  DCHECK(adapter_.is_bound());
}

BluetoothClassicMedium::~BluetoothClassicMedium() = default;

bool BluetoothClassicMedium::StartDiscovery(
    DiscoveryCallback discovery_callback) {
  if (!features::IsNearbyBluetoothClassicScanningEnabled()) {
    VLOG(1) << ": Classic scanning disabled, failing to StartDiscovery for BT "
               "Classic";
    return false;
  }

  if (adapter_observer_.is_bound() && discovery_callback_ &&
      discovery_session_.is_bound()) {
    LogStartDiscoveryResult(true);
    return true;
  }

  // TODO(hansberry): Verify with Nearby team if this is correct behavior.
  discovered_bluetooth_devices_map_.clear();

  bool success =
      adapter_->AddObserver(adapter_observer_.BindNewPipeAndPassRemote());
  if (!success) {
    adapter_observer_.reset();
    LogStartDiscoveryResult(false);
    return false;
  }

  mojo::PendingRemote<bluetooth::mojom::DiscoverySession> discovery_session;
  success =
      adapter_->StartDiscoverySession(kScanClientName, &discovery_session);

  if (!success || !discovery_session.is_valid()) {
    adapter_observer_.reset();
    LogStartDiscoveryResult(false);
    return false;
  }

  discovery_session_.Bind(std::move(discovery_session));
  discovery_session_.set_disconnect_handler(
      base::BindOnce(&BluetoothClassicMedium::DiscoveringChanged,
                     base::Unretained(this), /*discovering=*/false));

  discovery_callback_ = std::move(discovery_callback);
  stale_bluetooth_device_timer_.Reset();
  LogStartDiscoveryResult(true);
  return true;
}

bool BluetoothClassicMedium::StopDiscovery() {
  // TODO(hansberry): Verify with Nearby team if this is correct behavior:
  // Do not clear |discovered_bluetooth_devices_map_| because the caller still
  // needs references to BluetoothDevices to remain valid.

  bool stop_discovery_success = true;
  if (discovery_session_) {
    bool message_success = discovery_session_->Stop(&stop_discovery_success);
    stop_discovery_success = stop_discovery_success && message_success;
  }

  adapter_observer_.reset();
  discovery_callback_.reset();
  discovery_session_.reset();
  stale_bluetooth_device_timer_.Stop();

  LogStopDiscoveryResult(stop_discovery_success);
  return stop_discovery_success;
}

std::unique_ptr<api::BluetoothSocket> BluetoothClassicMedium::ConnectToService(
    api::BluetoothDevice& remote_device,
    const std::string& service_uuid,
    CancellationFlag* cancellation_flag) {
  if (cancellation_flag && cancellation_flag->Cancelled()) {
    return nullptr;
  }

  const std::string& address = remote_device.GetMacAddress();

  auto start_time = base::TimeTicks::Now();
  bluetooth::mojom::ConnectToServiceResultPtr result;
  bool success = adapter_->ConnectToServiceInsecurely(
      address, device::BluetoothUUID(service_uuid),
      /*should_unbond_on_error=*/true, &result);

  if (success && result) {
    LogConnectToServiceDuration(base::TimeTicks::Now() - start_time);
    LogConnectToServiceResult(true);
    return std::make_unique<chrome::BluetoothSocket>(
        remote_device, std::move(result->socket),
        std::move(result->receive_stream), std::move(result->send_stream));
  }

  LogConnectToServiceResult(false);
  return nullptr;
}

std::unique_ptr<api::BluetoothServerSocket>
BluetoothClassicMedium::ListenForService(const std::string& service_name,
                                         const std::string& service_uuid) {
  mojo::PendingRemote<bluetooth::mojom::ServerSocket> server_socket;
  bool success = adapter_->CreateRfcommServiceInsecurely(
      service_name, device::BluetoothUUID(service_uuid), &server_socket);

  if (success && server_socket) {
    LogListenForServiceResult(true);
    return std::make_unique<chrome::BluetoothServerSocket>(
        std::move(server_socket));
  }

  LogListenForServiceResult(false);
  return nullptr;
}

std::unique_ptr<api::BluetoothPairing>
BluetoothClassicMedium::CreatePairing(api::BluetoothDevice& remote_device) {
  // TODO(b/280656073): Add Chromium implementation for BluetoothPairing.
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothDevice* BluetoothClassicMedium::GetRemoteDevice(
    const std::string& mac_address) {
  auto it = discovered_bluetooth_devices_map_.find(mac_address);
  if (it != discovered_bluetooth_devices_map_.end())
    return &it->second;

  // If a device with |mac_address| has not been found, Nearby Connections
  // is attempting to connect to a device with |mac_adress| which is not
  // discoverable. Create a placeholder BluetoothDevice to be used by
  // ConnectToService().
  bluetooth::mojom::DeviceInfoPtr device = bluetooth::mojom::DeviceInfo::New();
  device->address = mac_address;
  return &discovered_bluetooth_devices_map_
              .emplace(mac_address, std::move(device))
              .first->second;
}

void BluetoothClassicMedium::PresentChanged(bool present) {
  // TODO(hansberry): It is unclear to me how the API implementation can signal
  // to Core that |present| has become unexpectedly false. Need to ask
  // Nearby team.
  if (!present)
    StopDiscovery();
}

void BluetoothClassicMedium::PoweredChanged(bool powered) {
  // TODO(hansberry): It is unclear to me how the API implementation can signal
  // to Core that |powered| has become unexpectedly false. Need to ask
  // Nearby team.
  if (!powered)
    StopDiscovery();
}

void BluetoothClassicMedium::DiscoverableChanged(bool discoverable) {
  // Do nothing. BluetoothClassicMedium is not responsible for managing
  // discoverable state.
  NOTIMPLEMENTED();
}

void BluetoothClassicMedium::DiscoveringChanged(bool discovering) {
  // TODO(hansberry): It is unclear to me how the API implementation can signal
  // to Core that |discovering| has become unexpectedly false. Need to ask
  // Nearby team.
  if (!discovering) {
    StopDiscovery();
  }
}

void BluetoothClassicMedium::DeviceAdded(
    bluetooth::mojom::DeviceInfoPtr device) {
  if (!adapter_observer_.is_bound() || !discovery_callback_ ||
      !discovery_session_.is_bound()) {
    return;
  }

  // Best-effort attempt to filter out BLE advertisements. BLE advertisements
  // represented as "devices" may have their |name| set if the system has
  // created a GATT connection to the advertiser, but all BT Classic devices
  // that we are interested in must have their |name| set. See BleMedium
  // for separate discovery of BLE advertisements (BlePeripherals).
  if (!device->name) {
    return;
  }

  const std::string& address = device->address;
  if (base::Contains(discovered_bluetooth_devices_map_, address)) {
    auto& bluetooth_device = discovered_bluetooth_devices_map_.at(address);
    bool name_changed = device->name.has_value() &&
                        device->name.value() != bluetooth_device.GetName();
    bluetooth_device.UpdateDevice(std::move(device), base::TimeTicks::Now());
    if (name_changed) {
      discovery_callback_->device_name_changed_cb(bluetooth_device);
    }
  } else {
    discovered_bluetooth_devices_map_.emplace(
        std::piecewise_construct, std::make_tuple(address),
        std::make_tuple(std::move(device), base::TimeTicks::Now()));
    discovery_callback_->device_discovered_cb(
        discovered_bluetooth_devices_map_.at(address));
  }
}

void BluetoothClassicMedium::DeviceChanged(
    bluetooth::mojom::DeviceInfoPtr device) {
  DeviceAdded(std::move(device));
}

void BluetoothClassicMedium::DeviceRemoved(
    bluetooth::mojom::DeviceInfoPtr device) {
  if (!adapter_observer_.is_bound() || !discovery_callback_ ||
      !discovery_session_.is_bound()) {
    return;
  }

  const std::string& address = device->address;
  if (!base::Contains(discovered_bluetooth_devices_map_, address))
    return;

  discovery_callback_->device_lost_cb(
      discovered_bluetooth_devices_map_.at(address));
  discovered_bluetooth_devices_map_.erase(address);
}

void BluetoothClassicMedium::RemoveStaleBluetoothDevices() {
  base::TimeTicks earliest_acceptable_discovery_time =
      base::TimeTicks::Now() - kStaleBluetoothDeviceTimeout;
  auto it = discovered_bluetooth_devices_map_.begin();
  while (it != discovered_bluetooth_devices_map_.end()) {
    if (it->second.GetLastDiscoveredTime().has_value() &&
        it->second.GetLastDiscoveredTime().value() <
            earliest_acceptable_discovery_time) {
      if (discovery_callback_) {
        discovery_callback_->device_lost_cb(it->second);
      }
      it = discovered_bluetooth_devices_map_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace nearby::chrome
