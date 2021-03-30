// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_

#include <stdint.h>
#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/thread_annotations.h"
#include "chrome/services/sharing/nearby/nearby_connections_stream_buffer_manager.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/services/nearby/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/cpp/core/internal/service_controller.h"

namespace location {
namespace nearby {
namespace connections {

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
      mojom::NearbyConnectionsDependenciesPtr dependencies,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      base::OnceClosure on_disconnect);

  NearbyConnections(const NearbyConnections&) = delete;
  NearbyConnections& operator=(const NearbyConnections&) = delete;
  ~NearbyConnections() override;

  // Should only be used by objects within lifetime of NearbyConnections.
  static NearbyConnections& GetInstance();

  // May return an unbound Remote if Nearby Connections was not provided an
  // Adapter (likely because this device does not support Bluetooth).
  const mojo::SharedRemote<bluetooth::mojom::Adapter>& bluetooth_adapter()
      const {
    return bluetooth_adapter_;
  }

  const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager()
      const {
    return socket_manager_;
  }
  const mojo::SharedRemote<
      location::nearby::connections::mojom::MdnsResponderFactory>&
  mdns_responder_factory() const {
    return mdns_responder_factory_;
  }
  const mojo::SharedRemote<sharing::mojom::IceConfigFetcher>&
  ice_config_fetcher() const {
    return ice_config_fetcher_;
  }
  const mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>&
  webrtc_signaling_messenger() const {
    return webrtc_signaling_messenger_;
  }

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

  // Returns the file associated with |payload_id| for InputFile.
  base::File ExtractInputFile(int64_t payload_id);

  // Returns the file associated with |payload_id| for OutputFile.
  base::File ExtractOutputFile(int64_t payload_id);

  // Returns the task runner for the thread that created |this|.
  scoped_refptr<base::SingleThreadTaskRunner> GetThreadTaskRunner();

  void SetServiceControllerForTesting(
      std::unique_ptr<ServiceController> service_controller);

 private:
  // These values are used for metrics. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class MojoDependencyName {
    kNearbyConnections = 0,
    kBluetoothAdapter = 1,
    kSocketManager = 2,
    kMdnsResponder = 3,
    kIceConfigFetcher = 4,
    kWebRtcSignalingMessenger = 5,
    kMaxValue = kWebRtcSignalingMessenger
  };

  Core* GetCore(const std::string& service_id);

  std::string GetMojoDependencyName(MojoDependencyName dependency_name);

  void OnDisconnect(MojoDependencyName dependency_name);

  mojo::Receiver<mojom::NearbyConnections> nearby_connections_;
  base::OnceClosure on_disconnect_;

  // Medium dependencies. SharedRemote is used to ensure all calls are posted
  // to sequence binding the Remote.
  mojo::SharedRemote<bluetooth::mojom::Adapter> bluetooth_adapter_;
  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager_;
  mojo::SharedRemote<location::nearby::connections::mojom::MdnsResponderFactory>
      mdns_responder_factory_;
  mojo::SharedRemote<sharing::mojom::IceConfigFetcher> ice_config_fetcher_;
  mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>
      webrtc_signaling_messenger_;

  std::unique_ptr<ServiceController> service_controller_;

  // Map from service ID to the Core object to be used for that service. Each
  // service uses its own Core object, but all Core objects share the underlying
  // ServiceController instance.
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

  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner_;

  base::WeakPtrFactory<NearbyConnections> weak_ptr_factory_{this};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_CONNECTIONS_H_
