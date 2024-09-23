// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "chrome/services/sharing/nearby/nearby_connections_stream_buffer_manager.h"
#include "chrome/services/sharing/nearby/nearby_shared_remotes.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/nearby/src/connections/implementation/service_controller_router.h"
#include "third_party/nearby/src/presence/presence_device.h"
#include "third_party/nearby/src/presence/presence_device_provider.h"

namespace nearby::connections {

class Core;

// Implementation of the NearbyConnections mojo interface.
// This class acts as a bridge to the NearbyConnections library which is pulled
// in as a third_party dependency. It handles the translation from mojo calls to
// native callbacks and types that the library expects. This class runs in a
// sandboxed process and is called from the browser process. The passed |host|
// interface is implemented in the browser process and is used to fetch runtime
// dependencies to other mojo interfaces like Bluetooth or WiFi LAN.
class NearbyConnections : public mojom::NearbyConnections {
 public:
  // Creates a new instance of the NearbyConnections library. This will allocate
  // and initialize a new instance and hold on to the passed mojo pipes.
  // |on_disconnect| is called when either mojo interface disconnects and should
  // destroy this instance.
  NearbyConnections(
      mojo::PendingReceiver<mojom::NearbyConnections> nearby_connections,
      NearbyDeviceProvider* presence_device_provider,
      nearby::api::LogMessage::Severity min_log_severity,
      base::OnceClosure on_disconnect);

  NearbyConnections(const NearbyConnections&) = delete;
  NearbyConnections& operator=(const NearbyConnections&) = delete;
  ~NearbyConnections() override;

  // Should only be used by objects within lifetime of NearbyConnections.
  static NearbyConnections& GetInstance();

  // mojom::NearbyConnections:
  void StartAdvertising(
      const std::string& service_id,
      const std::vector<uint8_t>& endpoint_info,
      mojom::AdvertisingOptionsPtr options,
      mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
      StartAdvertisingCallback callback) override;
  void StopAdvertising(const std::string& service_id,
                       StopAdvertisingCallback callback) override;
  void StartDiscovery(
      const std::string& service_id,
      mojom::DiscoveryOptionsPtr options,
      mojo::PendingRemote<mojom::EndpointDiscoveryListener> listener,
      StartDiscoveryCallback callback) override;
  void StopDiscovery(const std::string& service_id,
                     StopDiscoveryCallback callback) override;
  void InjectBluetoothEndpoint(
      const std::string& service_id,
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info,
      const std::vector<uint8_t>& remote_bluetooth_mac_address,
      InjectBluetoothEndpointCallback callback) override;
  void RequestConnection(
      const std::string& service_id,
      const std::vector<uint8_t>& endpoint_info,
      const std::string& endpoint_id,
      mojom::ConnectionOptionsPtr options,
      mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
      RequestConnectionCallback callback) override;
  void DisconnectFromEndpoint(const std::string& service_id,
                              const std::string& endpoint_id,
                              DisconnectFromEndpointCallback callback) override;
  void AcceptConnection(const std::string& service_id,
                        const std::string& endpoint_id,
                        mojo::PendingRemote<mojom::PayloadListener> listener,
                        AcceptConnectionCallback callback) override;
  void RejectConnection(const std::string& service_id,
                        const std::string& endpoint_id,
                        RejectConnectionCallback callback) override;
  void SendPayload(const std::string& service_id,
                   const std::vector<std::string>& endpoint_ids,
                   mojom::PayloadPtr payload,
                   SendPayloadCallback callback) override;
  void CancelPayload(const std::string& service_id,
                     int64_t payload_id,
                     CancelPayloadCallback callback) override;
  void StopAllEndpoints(const std::string& service_id,
                        StopAllEndpointsCallback callback) override;
  void InitiateBandwidthUpgrade(
      const std::string& service_id,
      const std::string& endpoint_id,
      InitiateBandwidthUpgradeCallback callback) override;
  void RegisterPayloadFile(const std::string& service_id,
                           int64_t payload_id,
                           base::File input_file,
                           base::File output_file,
                           RegisterPayloadFileCallback callback) override;
  void RequestConnectionV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      mojom::ConnectionOptionsPtr connection_options,
      mojo::PendingRemote<mojom::ConnectionListenerV3> listener,
      RequestConnectionV3Callback callback) override;
  void AcceptConnectionV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      mojo::PendingRemote<mojom::PayloadListenerV3> listener,
      AcceptConnectionV3Callback callback) override;
  void RejectConnectionV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      RejectConnectionV3Callback callback) override;
  void DisconnectFromDeviceV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      DisconnectFromDeviceV3Callback callback) override;
  void RegisterServiceWithPresenceDeviceProvider(
      const std::string& service_id) override;

  // Returns the file associated with |payload_id| for InputFile.
  base::File ExtractInputFile(int64_t payload_id);

  // Returns the file associated with |payload_id| for OutputFile.
  base::File ExtractOutputFile(int64_t payload_id);

  // Returns the task runner for the thread that created |this|.
  scoped_refptr<base::SingleThreadTaskRunner> GetThreadTaskRunner();

  void SetServiceControllerRouterForTesting(
      std::unique_ptr<ServiceControllerRouter> service_controller_router);

 private:
  Core* GetCore(const std::string& service_id);

  const presence::PresenceDevice& GetPresenceDevice(
      const std::string& service_id,
      const std::string& endpoint_id) const;
  void RemovePresenceDevice(const std::string& service_id,
                            const std::string& endpoint_id);

  mojo::Receiver<mojom::NearbyConnections> nearby_connections_;

  // This field is only used in `RegisterServiceWithPresenceDeviceProvider()`
  // for authentication of connections when using Nearby Presence. Nearby
  // Connections clients who do not also use Nearby Presence should not call
  // this method.
  raw_ptr<NearbyDeviceProvider> presence_local_device_provider_;

  std::unique_ptr<ServiceControllerRouter> service_controller_router_;

  // Map from service ID to the Core object to be used for that service. Each
  // service uses its own Core object, but all Core objects share the underlying
  // ServiceControllerRouter instance.
  base::flat_map<std::string, std::unique_ptr<Core>> service_id_to_core_map_;

  // Handles incoming stream payloads. This object buffers partial streams as
  // they arrive and provides a getter for the final buffer when it is complete.
  NearbyConnectionsStreamBufferManager buffer_manager_;

  // input_file_map_ is accessed from background threads.
  base::Lock input_file_lock_;
  // A map of payload_id to file for InputFile.
  base::flat_map<int64_t, base::File> input_file_map_
      GUARDED_BY(input_file_lock_);

  // output_file_map_ is accessed from background threads.
  base::Lock output_file_lock_;
  // A map of payload_id to file for OutputFile.
  base::flat_map<int64_t, base::File> output_file_map_
      GUARDED_BY(output_file_lock_);

  // A map of outgoing connections to remote devices per service, keyed first by
  // `service_id`, and then as `endpoint_id` to `PresenceDevice`. This class
  // must own its `PresenceDevice` instances, because Nearby Connections' `Core`
  // object only accepts `PresenceDevice` references.
  base::flat_map<
      std::string,
      base::flat_map<std::string, std::unique_ptr<presence::PresenceDevice>>>
      service_id_to_endpoint_id_to_presence_devices_with_outgoing_connections_map_;

  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner_;

  base::WeakPtrFactory<NearbyConnections> weak_ptr_factory_{this};
};

}  // namespace nearby::connections

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
