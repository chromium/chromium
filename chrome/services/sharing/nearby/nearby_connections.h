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
#include "chrome/services/sharing/public/mojom/nearby_connections.mojom.h"
#include "chrome/services/sharing/public/mojom/webrtc_signaling_messenger.mojom.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/cpp/core_v2/core.h"

namespace location {
namespace nearby {
namespace connections {

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
      base::OnceClosure on_disconnect,
      std::unique_ptr<Core> core = std::make_unique<Core>());

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
  const mojo::SharedRemote<network::mojom::MdnsResponder>& mdns_responder()
      const {
    return mdns_responder_;
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
      const std::vector<uint8_t>& endpoint_info,
      const std::string& service_id,
      mojom::AdvertisingOptionsPtr options,
      mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
      StartAdvertisingCallback callback) override;
  void StopAdvertising(StopAdvertisingCallback callback) override;
  void StartDiscovery(
      const std::string& service_id,
      mojom::DiscoveryOptionsPtr options,
      mojo::PendingRemote<mojom::EndpointDiscoveryListener> listener,
      StartDiscoveryCallback callback) override;
  void StopDiscovery(StopDiscoveryCallback callback) override;
  void RequestConnection(
      const std::vector<uint8_t>& endpoint_info,
      const std::string& endpoint_id,
      mojom::ConnectionOptionsPtr options,
      mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
      RequestConnectionCallback callback) override;
  void DisconnectFromEndpoint(const std::string& endpoint_id,
                              DisconnectFromEndpointCallback callback) override;
  void AcceptConnection(const std::string& endpoint_id,
                        mojo::PendingRemote<mojom::PayloadListener> listener,
                        AcceptConnectionCallback callback) override;
  void RejectConnection(const std::string& endpoint_id,
                        RejectConnectionCallback callback) override;
  void SendPayload(const std::vector<std::string>& endpoint_ids,
                   mojom::PayloadPtr payload,
                   SendPayloadCallback callback) override;
  void CancelPayload(int64_t payload_id,
                     CancelPayloadCallback callback) override;
  void StopAllEndpoints(StopAllEndpointsCallback callback) override;
  void InitiateBandwidthUpgrade(
      const std::string& endpoint_id,
      InitiateBandwidthUpgradeCallback callback) override;
  void RegisterPayloadFile(int64_t payload_id,
                           base::File input_file,
                           base::File output_file,
                           RegisterPayloadFileCallback callback) override;

  // Returns the file associated with |payload_id| for InputFile.
  base::File ExtractInputFile(int64_t payload_id);

  // Returns the file associated with |payload_id| for OutputFile.
  base::File ExtractOutputFile(int64_t payload_id);

  // Returns the task runner for the thread that created |this|.
  scoped_refptr<base::SingleThreadTaskRunner> GetThreadTaskRunner();

 private:
  void OnDisconnect();

  mojo::Receiver<mojom::NearbyConnections> nearby_connections_;
  base::OnceClosure on_disconnect_;

  // Medium dependencies. SharedRemote is used to ensure all calls are posted
  // to sequence binding the Remote.
  mojo::SharedRemote<bluetooth::mojom::Adapter> bluetooth_adapter_;
  mojo::SharedRemote<network::mojom::P2PSocketManager> socket_manager_;
  mojo::SharedRemote<network::mojom::MdnsResponder> mdns_responder_;
  mojo::SharedRemote<sharing::mojom::IceConfigFetcher> ice_config_fetcher_;
  mojo::SharedRemote<sharing::mojom::WebRtcSignalingMessenger>
      webrtc_signaling_messenger_;

  // Core is thread-safe as its operations are always dispatched to a
  // single-thread executor.
  std::unique_ptr<Core> core_;

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
