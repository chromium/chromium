// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/nearby/src/internal/platform/implementation/platform.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/nearby/nearby_shared_remotes.h"
#include "chrome/services/sharing/nearby/platform/atomic_boolean.h"
#include "chrome/services/sharing/nearby/platform/atomic_uint32.h"
#include "chrome/services/sharing/nearby/platform/ble_medium.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_medium.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_classic_medium.h"
#include "chrome/services/sharing/nearby/platform/condition_variable.h"
#include "chrome/services/sharing/nearby/platform/count_down_latch.h"
#include "chrome/services/sharing/nearby/platform/credential_storage.h"
#include "chrome/services/sharing/nearby/platform/input_file.h"
#include "chrome/services/sharing/nearby/platform/log_message.h"
#include "chrome/services/sharing/nearby/platform/mutex.h"
#include "chrome/services/sharing/nearby/platform/output_file.h"
#include "chrome/services/sharing/nearby/platform/recursive_mutex.h"
#include "chrome/services/sharing/nearby/platform/scheduled_executor.h"
#include "chrome/services/sharing/nearby/platform/submittable_executor.h"
#include "chrome/services/sharing/nearby/platform/webrtc.h"
#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"
#include "chrome/services/sharing/nearby/platform/wifi_lan_medium.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/atomic_boolean.h"
#include "third_party/nearby/src/internal/platform/implementation/atomic_reference.h"
#include "third_party/nearby/src/internal/platform/implementation/ble.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"
#include "third_party/nearby/src/internal/platform/implementation/bluetooth_adapter.h"
#include "third_party/nearby/src/internal/platform/implementation/bluetooth_classic.h"
#include "third_party/nearby/src/internal/platform/implementation/condition_variable.h"
#include "third_party/nearby/src/internal/platform/implementation/count_down_latch.h"
#include "third_party/nearby/src/internal/platform/implementation/credential_storage.h"
#include "third_party/nearby/src/internal/platform/implementation/log_message.h"
#include "third_party/nearby/src/internal/platform/implementation/mutex.h"
#include "third_party/nearby/src/internal/platform/implementation/scheduled_executor.h"
#include "third_party/nearby/src/internal/platform/implementation/server_sync.h"
#include "third_party/nearby/src/internal/platform/implementation/shared/file.h"
#include "third_party/nearby/src/internal/platform/implementation/submittable_executor.h"
#include "third_party/nearby/src/internal/platform/implementation/webrtc.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_direct.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_hotspot.h"

namespace nearby::chrome {

// Minimal `WifiMedium` implementation required to allow WiFi Direct.
class WifiMedium : public api::WifiMedium {
 public:
  WifiMedium() {
    // The official WifiDirectMedium implementation queries the platform layer
    // to determine if the device supports WiFi Direct, so this capabilities
    // value is not actually used to determine support.
    capabilities_.support_wifi_direct = true;
  }

  // api::WifiMedium
  bool IsInterfaceValid() const override {
    // This call is required for the WifiDirectMedium to trigger properly.
    return true;
  }

  api::WifiCapability& GetCapability() override {
    NOTIMPLEMENTED();
    return capabilities_;
  }

  api::WifiInformation& GetInformation() override {
    NOTIMPLEMENTED();
    return information_;
  }

  bool Scan(const ScanResultCallback& scan_result_callback) override {
    NOTIMPLEMENTED();
    return false;
  }

  api::WifiConnectionStatus ConnectToNetwork(
      absl::string_view ssid,
      absl::string_view password,
      api::WifiAuthType auth_type) override {
    NOTIMPLEMENTED();
    return api::WifiConnectionStatus::kUnknown;
  }

  bool VerifyInternetConnectivity() override {
    NOTIMPLEMENTED();
    return false;
  }

  std::string GetIpAddress() override {
    NOTIMPLEMENTED();
    return std::string();
  }

 private:
  api::WifiCapability capabilities_;
  api::WifiInformation information_;
};

}  // namespace nearby::chrome

namespace nearby::api {

int GetCurrentTid() {
  // SubmittableExecutor and ScheduledExecutor does not own a thread pool
  // directly nor manages threads, thus cannot support this debug feature.
  return 0;
}

std::string ImplementationPlatform::GetCustomSavePath(
    const std::string& parent_folder,
    const std::string& file_name) {
  // This should return the <saved_custom_path>/file_name. For now we will
  // just return an empty string, since chrome doesn't call this yet.
  // TODO(b/223710122): Eventually chrome should implement this method.
  NOTIMPLEMENTED();
  return std::string();
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string& parent_folder,
    const std::string& file_name) {
  // This should return the <download_path>/parent_folder/file_name. For now we
  // will just return an empty string, since chrome doesn't call this yet.
  // TODO(b/223710122): Eventually chrome should implement this method.
  NOTIMPLEMENTED();
  return std::string();
}

OSName ImplementationPlatform::GetCurrentOS() {
  return OSName::kChromeOS;
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<chrome::SubmittableExecutor>(
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED));
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(int max_concurrency) {
  // We ignore |max_concurrency| and submit tasks to the main process thread
  // pool. Just before the task starts executing we enter a WILL_BLOCK scope
  // which signals to the thread pool to allocate a new thread if needed. This
  // gives the executor an effective thread count of whatever they need up to
  // the max thread pool size of 255.
  return std::make_unique<chrome::SubmittableExecutor>(
      base::ThreadPool::CreateTaskRunner({base::MayBlock()}));
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  // TODO(crbug.com/40134072): Figure out if task runner needs to run in main
  // thread.
  return std::make_unique<chrome::ScheduledExecutor>(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t initial_value) {
  return std::make_unique<chrome::AtomicUint32>(initial_value);
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();
  if (nearby_shared_remotes &&
      nearby_shared_remotes->bluetooth_adapter.is_bound()) {
    return std::make_unique<chrome::BluetoothAdapter>(
        nearby_shared_remotes->bluetooth_adapter);
  }
  return nullptr;
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return std::make_unique<chrome::CountDownLatch>(count);
}

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return std::make_unique<chrome::AtomicBoolean>(initial_value);
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    std::int64_t payload_id,
    std::int64_t total_size) {
  auto& connections = connections::NearbyConnections::GetInstance();
  return std::make_unique<chrome::InputFile>(
      connections.ExtractInputFile(payload_id));
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    const std::string& file_path,
    size_t size) {
  // This constructor is not called by Chrome. Returning nullptr, just in case.
  // TODO(b/223710122): Eventually chrome should implement and use this
  // constructor exclusively.
  NOTIMPLEMENTED();
  return nullptr;
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    std::int64_t payload_id) {
  auto& connections = connections::NearbyConnections::GetInstance();
  return std::make_unique<chrome::OutputFile>(
      connections.ExtractOutputFile(payload_id));
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    const std::string& file_path) {
  // This constructor is not called by Chrome. Returning nullptr, just in case.
  // TODO(b/223710122): Eventually chrome should implement and use this
  // constructor exclusively.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file,
    int line,
    LogMessage::Severity severity) {
  return std::make_unique<chrome::LogMessage>(file, line, severity);
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    api::BluetoothAdapter& adapter) {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();
  // Ignore the provided |adapter| argument; it is a reference to the object
  // created by ImplementationPlatform::CreateBluetoothAdapter(). Instead,
  // directly use the cached bluetooth::mojom::Adapter.
  if (nearby_shared_remotes &&
      nearby_shared_remotes->bluetooth_adapter.is_bound()) {
    return std::make_unique<chrome::BluetoothClassicMedium>(
        nearby_shared_remotes->bluetooth_adapter);
  }
  return nullptr;
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    api::BluetoothAdapter& adapter) {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();
  // Ignore the provided |adapter| argument; it is a reference to the object
  // created by ImplementationPlatform::CreateBluetoothAdapter(). Instead,
  // directly use the cached bluetooth::mojom::Adapter.
  if (nearby_shared_remotes &&
      nearby_shared_remotes->bluetooth_adapter.is_bound()) {
    return std::make_unique<chrome::BleMedium>(
        nearby_shared_remotes->bluetooth_adapter);
  }
  return nullptr;
}

std::unique_ptr<ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium(
    api::BluetoothAdapter& adapter) {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();
  // Ignore the provided |adapter| argument; it is a reference to the object
  // created by ImplementationPlatform::CreateBluetoothAdapter(). Instead,
  // directly use the cached bluetooth::mojom::Adapter.
  if (nearby_shared_remotes &&
      nearby_shared_remotes->bluetooth_adapter.is_bound() &&
      features::IsNearbyBleV2Enabled()) {
    return std::make_unique<chrome::BleV2Medium>(
        nearby_shared_remotes->bluetooth_adapter);
  }
  return nullptr;
}

std::unique_ptr<api::CredentialStorage>
ImplementationPlatform::CreateCredentialStorage() {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();
  if (nearby_shared_remotes &&
      nearby_shared_remotes->nearby_presence_credential_storage.is_bound()) {
    return std::make_unique<nearby::chrome::CredentialStorage>(
        nearby_shared_remotes->nearby_presence_credential_storage);
  }
  return nullptr;
}

std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return nullptr;
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::make_unique<chrome::WifiMedium>();
}

std::unique_ptr<WifiDirectMedium>
ImplementationPlatform::CreateWifiDirectMedium() {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();
  if (!nearby_shared_remotes) {
    return nullptr;
  }

  // TODO(b/340273442): This should always be bound when the WifiDirect feature
  // flag is enabled. Update logging to ERROR after launch.
  if (!nearby_shared_remotes->wifi_direct_manager.is_bound()) {
    VLOG(1) << "WifiDirectManager not bound. Returning null WifiDirect medium";
    return nullptr;
  }

  if (!nearby_shared_remotes->wifi_direct_firewall_hole_factory.is_bound()) {
    VLOG(1)
        << "FirewallHoleFactory not bound. Returning null WifiDirect medium";
    return nullptr;
  }

  return std::make_unique<chrome::WifiDirectMedium>(
      std::move(nearby_shared_remotes->wifi_direct_manager),
      std::move(nearby_shared_remotes->wifi_direct_firewall_hole_factory));
}

std::unique_ptr<WifiHotspotMedium>
ImplementationPlatform::CreateWifiHotspotMedium() {
  return nullptr;
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();
  if (!nearby_shared_remotes) {
    return nullptr;
  }

  // TODO(https://crbug.com/1261238): This should always be bound when the
  // WifiLan feature flag is enabled. Update logging to ERROR after launch.
  const mojo::SharedRemote<chromeos::network_config::mojom::CrosNetworkConfig>&
      cros_network_config = nearby_shared_remotes->cros_network_config;
  if (!cros_network_config.is_bound()) {
    VLOG(1) << "CrosNetworkConfig not bound. Returning null WifiLan medium";
    return nullptr;
  }

  // TODO(https://crbug.com/1261238): This should always be bound when the
  // WifiLan feature flag is enabled. Update logging to ERROR after launch.
  const mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>&
      firewall_hole_factory = nearby_shared_remotes->firewall_hole_factory;
  if (!firewall_hole_factory.is_bound()) {
    VLOG(1) << "FirewallHoleFactory not bound. Returning null WifiLan medium";
    return nullptr;
  }

  // TODO(https://crbug.com/1261238): This should always be bound when the
  // WifiLan feature flag is enabled. Update logging to ERROR after launch.
  const mojo::SharedRemote<::sharing::mojom::TcpSocketFactory>&
      tcp_socket_factory = nearby_shared_remotes->tcp_socket_factory;
  if (!tcp_socket_factory.is_bound()) {
    VLOG(1) << "TcpSocketFactory not bound. Returning null WifiLan medium";
    return nullptr;
  }

  const mojo::SharedRemote<::sharing::mojom::MdnsManager>& mdns_manager =
      nearby_shared_remotes->mdns_manager;
  if (features::IsNearbyMdnsEnabled() && !mdns_manager.is_bound()) {
    LOG(ERROR) << "MdnsManager not bound. Returning null WifiLan medium";
    return nullptr;
  }

  return std::make_unique<chrome::WifiLanMedium>(
      tcp_socket_factory, cros_network_config, firewall_hole_factory,
      mdns_manager);
}

std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  nearby::NearbySharedRemotes* nearby_shared_remotes =
      nearby::NearbySharedRemotes::GetInstance();

  if (!nearby_shared_remotes) {
    LOG(ERROR) << "No NearbySharedRemotes instance. Returning null medium.";
    return nullptr;
  }

  const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager =
      nearby_shared_remotes->socket_manager;
  const mojo::SharedRemote<::sharing::mojom::MdnsResponderFactory>&
      mdns_responder_factory = nearby_shared_remotes->mdns_responder_factory;
  const mojo::SharedRemote<::sharing::mojom::IceConfigFetcher>&
      ice_config_fetcher = nearby_shared_remotes->ice_config_fetcher;
  const mojo::SharedRemote<::sharing::mojom::WebRtcSignalingMessenger>&
      messenger = nearby_shared_remotes->webrtc_signaling_messenger;

  auto log_error = [](std::string dependency_name) {
    LOG(ERROR) << "Webrtc dependency [" << dependency_name
               << "] is not bound. Returning null medium.";
  };

  if (!socket_manager.is_bound()) {
    log_error("socket_manager");
    return nullptr;
  }

  if (!mdns_responder_factory.is_bound()) {
    log_error("mdns_responder_factory");
    return nullptr;
  }

  if (!ice_config_fetcher.is_bound()) {
    log_error("ice_config_fetcher");
    return nullptr;
  }

  if (!messenger.is_bound()) {
    log_error("messenger");
    return nullptr;
  }

  auto& connections = connections::NearbyConnections::GetInstance();

  return std::make_unique<chrome::WebRtcMedium>(
      socket_manager, mdns_responder_factory, ice_config_fetcher, messenger,
      connections.GetThreadTaskRunner());
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  // Chrome does not support unchecked Mutex in debug mode, therefore
  // chrome::Mutex is used for both kRegular and kRegularNoCheck.
  if (mode == Mutex::Mode::kRecursive) {
    return std::make_unique<chrome::RecursiveMutex>();
  } else {
    return std::make_unique<chrome::Mutex>();
  }
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::make_unique<chrome::ConditionVariable>(
      static_cast<chrome::Mutex*>(mutex));
}

}  // namespace nearby::api
