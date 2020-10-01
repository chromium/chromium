// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/nearby/src/cpp/platform_v2/api/platform.h"

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chrome/services/sharing/nearby/platform_v2/atomic_boolean.h"
#include "chrome/services/sharing/nearby/platform_v2/atomic_uint32.h"
#include "chrome/services/sharing/nearby/platform_v2/ble_medium.h"
#include "chrome/services/sharing/nearby/platform_v2/bluetooth_adapter.h"
#include "chrome/services/sharing/nearby/platform_v2/bluetooth_classic_medium.h"
#include "chrome/services/sharing/nearby/platform_v2/condition_variable.h"
#include "chrome/services/sharing/nearby/platform_v2/count_down_latch.h"
#include "chrome/services/sharing/nearby/platform_v2/input_file.h"
#include "chrome/services/sharing/nearby/platform_v2/log_message.h"
#include "chrome/services/sharing/nearby/platform_v2/mutex.h"
#include "chrome/services/sharing/nearby/platform_v2/output_file.h"
#include "chrome/services/sharing/nearby/platform_v2/recursive_mutex.h"
#include "chrome/services/sharing/nearby/platform_v2/scheduled_executor.h"
#include "chrome/services/sharing/nearby/platform_v2/submittable_executor.h"
#include "chrome/services/sharing/nearby/platform_v2/webrtc.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/cpp/platform_v2/api/atomic_boolean.h"
#include "third_party/nearby/src/cpp/platform_v2/api/atomic_reference.h"
#include "third_party/nearby/src/cpp/platform_v2/api/ble.h"
#include "third_party/nearby/src/cpp/platform_v2/api/ble_v2.h"
#include "third_party/nearby/src/cpp/platform_v2/api/bluetooth_adapter.h"
#include "third_party/nearby/src/cpp/platform_v2/api/bluetooth_classic.h"
#include "third_party/nearby/src/cpp/platform_v2/api/condition_variable.h"
#include "third_party/nearby/src/cpp/platform_v2/api/count_down_latch.h"
#include "third_party/nearby/src/cpp/platform_v2/api/log_message.h"
#include "third_party/nearby/src/cpp/platform_v2/api/mutex.h"
#include "third_party/nearby/src/cpp/platform_v2/api/scheduled_executor.h"
#include "third_party/nearby/src/cpp/platform_v2/api/server_sync.h"
#include "third_party/nearby/src/cpp/platform_v2/api/submittable_executor.h"
#include "third_party/nearby/src/cpp/platform_v2/api/webrtc.h"
#include "third_party/nearby/src/cpp/platform_v2/api/wifi.h"
#include "third_party/nearby/src/cpp/platform_v2/impl/shared/file.h"

namespace location {
namespace nearby {
namespace api {

int GetCurrentTid() {
  // SubmittableExecutor and ScheduledExecutor does not own a thread pool
  // directly nor manages threads, thus cannot support this debug feature.
  return 0;
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
  // Chrome TaskRunner does not support |max_concurrency|. Simply use our
  // SingleThreadExecutor.
  return CreateSingleThreadExecutor();
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  // TODO(crbug/1091190): Figure out if task runner needs to run in main thread.
  return std::make_unique<chrome::ScheduledExecutor>(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t initial_value) {
  return std::make_unique<chrome::AtomicUint32>(initial_value);
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  auto& connections = connections::NearbyConnections::GetInstance();
  const mojo::SharedRemote<bluetooth::mojom::Adapter>& bluetooth_adapter =
      connections.bluetooth_adapter();

  if (!bluetooth_adapter.is_bound())
    return nullptr;

  return std::make_unique<chrome::BluetoothAdapter>(bluetooth_adapter);
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return std::make_unique<chrome::CountDownLatch>(count);
}

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return std::make_unique<chrome::AtomicBoolean>(initial_value);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    std::int64_t payload_id,
    std::int64_t total_size) {
  auto& connections = connections::NearbyConnections::GetInstance();
  return std::make_unique<chrome::InputFile>(
      connections.ExtractInputFile(payload_id));
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    std::int64_t payload_id) {
  auto& connections = connections::NearbyConnections::GetInstance();
  return std::make_unique<chrome::OutputFile>(
      connections.ExtractOutputFile(payload_id));
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
  // Ignore the provided |adapter| argument. It provides no interface useful
  // to implement chrome::BluetoothClassicMedium.

  auto& connections = connections::NearbyConnections::GetInstance();
  const mojo::SharedRemote<bluetooth::mojom::Adapter>& bluetooth_adapter =
      connections.bluetooth_adapter();

  if (!bluetooth_adapter.is_bound())
    return nullptr;

  return std::make_unique<chrome::BluetoothClassicMedium>(bluetooth_adapter);
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    api::BluetoothAdapter& adapter) {
  // Ignore the provided |adapter| argument. It provides no interface useful
  // to implement chrome::BleMedium.

  auto& connections = connections::NearbyConnections::GetInstance();
  const mojo::SharedRemote<bluetooth::mojom::Adapter>& bluetooth_adapter =
      connections.bluetooth_adapter();

  if (!bluetooth_adapter.is_bound())
    return nullptr;

  return std::make_unique<chrome::BleMedium>(bluetooth_adapter);
}

std::unique_ptr<ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium(
    api::BluetoothAdapter& adapter) {
  // Do nothing. ble_v2::BleMedium is not yet supported in Chrome Nearby.
  return nullptr;
}

std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return nullptr;
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return nullptr;
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return nullptr;
}

std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  auto& connections = connections::NearbyConnections::GetInstance();

  const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager =
      connections.socket_manager();
  const mojo::SharedRemote<network::mojom::MdnsResponder>& mdns_responder =
      connections.mdns_responder();
  const mojo::SharedRemote<sharing::mojom::IceConfigFetcher>&
      ice_config_fetcher = connections.ice_config_fetcher();
  const mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>&
      messenger = connections.webrtc_signaling_messenger();

  if (!socket_manager.is_bound() || !mdns_responder.is_bound() ||
      !ice_config_fetcher.is_bound() || !messenger.is_bound()) {
    return nullptr;
  }

  return std::make_unique<chrome::WebRtcMedium>(
      socket_manager, mdns_responder, ice_config_fetcher, messenger,
      connections.GetThreadTaskRunner());
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  // Chrome does not support unchecked Mutex in debug mode, therefore
  // chrome::Mutex is used for both kRegular and kRegularNoCheck.
  if (mode == Mutex::Mode::kRecursive)
    return std::make_unique<chrome::RecursiveMutex>();
  else
    return std::make_unique<chrome::Mutex>();
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::make_unique<chrome::ConditionVariable>(
      static_cast<chrome::Mutex*>(mutex));
}

}  // namespace api
}  // namespace nearby
}  // namespace location
