// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/services/sharing/nearby/nearby_connections_conversions.h"
#include "chrome/services/sharing/nearby/platform/input_file.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "third_party/nearby/src/cpp/core/core.h"
#include "third_party/nearby/src/cpp/core/internal/offline_service_controller.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

// Delegates all ServiceController calls to the ServiceController instance
// passed to its constructor. This proxy class is required because although we
// share one ServiceController among multiple Cores, each Core takes ownership
// of the pointer that it is provided. Using this proxy allows each Core to
// delete the pointer it is provided without deleting the shared instance.
class ServiceControllerProxy : public ServiceController {
 public:
  explicit ServiceControllerProxy(
      std::unique_ptr<ServiceController>& inner_service_controller)
      : inner_service_controller_(inner_service_controller) {}
  ~ServiceControllerProxy() override = default;

  // ServiceController:
  Status StartAdvertising(ClientProxy* client,
                          const std::string& service_id,
                          const ConnectionOptions& options,
                          const ConnectionRequestInfo& info) override {
    if (!inner_service_controller_)
      return {Status::kError};
    return inner_service_controller_->StartAdvertising(client, service_id,
                                                       options, info);
  }

  void StopAdvertising(ClientProxy* client) override {
    if (!inner_service_controller_)
      return;
    inner_service_controller_->StopAdvertising(client);
  }

  Status StartDiscovery(ClientProxy* client,
                        const std::string& service_id,
                        const ConnectionOptions& options,
                        const DiscoveryListener& listener) override {
    if (!inner_service_controller_)
      return {Status::kError};
    return inner_service_controller_->StartDiscovery(client, service_id,
                                                     options, listener);
  }

  void StopDiscovery(ClientProxy* client) override {
    if (!inner_service_controller_)
      return;
    inner_service_controller_->StopDiscovery(client);
  }

  void InjectEndpoint(ClientProxy* client,
                      const std::string& service_id,
                      const OutOfBandConnectionMetadata& metadata) override {
    if (!inner_service_controller_)
      return;
    inner_service_controller_->InjectEndpoint(client, service_id, metadata);
  }

  Status RequestConnection(ClientProxy* client,
                           const std::string& endpoint_id,
                           const ConnectionRequestInfo& info,
                           const ConnectionOptions& options) override {
    if (!inner_service_controller_)
      return {Status::kError};
    return inner_service_controller_->RequestConnection(client, endpoint_id,
                                                        info, options);
  }

  Status AcceptConnection(ClientProxy* client,
                          const std::string& endpoint_id,
                          const PayloadListener& listener) override {
    return inner_service_controller_->AcceptConnection(client, endpoint_id,
                                                       listener);
  }

  Status RejectConnection(ClientProxy* client,
                          const std::string& endpoint_id) override {
    if (!inner_service_controller_)
      return {Status::kError};
    return inner_service_controller_->RejectConnection(client, endpoint_id);
  }

  void InitiateBandwidthUpgrade(ClientProxy* client,
                                const std::string& endpoint_id) override {
    if (!inner_service_controller_)
      return;
    inner_service_controller_->InitiateBandwidthUpgrade(client, endpoint_id);
  }

  void SendPayload(ClientProxy* client,
                   const std::vector<std::string>& endpoint_ids,
                   Payload payload) override {
    if (!inner_service_controller_)
      return;
    inner_service_controller_->SendPayload(client, endpoint_ids,
                                           std::move(payload));
  }

  Status CancelPayload(ClientProxy* client, Payload::Id payload_id) override {
    if (!inner_service_controller_)
      return {Status::kError};
    return inner_service_controller_->CancelPayload(client,
                                                    std::move(payload_id));
  }

  void DisconnectFromEndpoint(ClientProxy* client,
                              const std::string& endpoint_id) override {
    if (!inner_service_controller_)
      return;
    inner_service_controller_->DisconnectFromEndpoint(client, endpoint_id);
  }

  void Stop() override {
    // TODO(https://crbug.com/1182428): we purposefully do nothing here so we
    // don't shutdown the share OfflineServiceController.
  }

 private:
  // This is intentionally a reference to the unique_ptr owned by
  // NearbyConnections. During the shutdown flow, NearbyConnection will clean up
  // it's service controller before this proxy is destroyed. The reference
  // allows us to stop forwarding calls if NearbyConnections has already cleaned
  // it up.
  std::unique_ptr<ServiceController>& inner_service_controller_;
};

ConnectionRequestInfo CreateConnectionRequestInfo(
    const std::vector<uint8_t>& endpoint_info,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener) {
  mojo::SharedRemote<mojom::ConnectionLifecycleListener> remote(
      std::move(listener));
  return ConnectionRequestInfo{
      .endpoint_info = ByteArrayFromMojom(endpoint_info),
      .listener = {
          .initiated_cb =
              [remote](const std::string& endpoint_id,
                       const ConnectionResponseInfo& info) {
                if (!remote)
                  return;

                remote->OnConnectionInitiated(
                    endpoint_id,
                    mojom::ConnectionInfo::New(
                        info.authentication_token,
                        ByteArrayToMojom(info.raw_authentication_token),
                        ByteArrayToMojom(info.remote_endpoint_info),
                        info.is_incoming_connection));
              },
          .accepted_cb =
              [remote](const std::string& endpoint_id) {
                if (!remote)
                  return;

                remote->OnConnectionAccepted(endpoint_id);
              },
          .rejected_cb =
              [remote](const std::string& endpoint_id, Status status) {
                if (!remote)
                  return;

                remote->OnConnectionRejected(endpoint_id,
                                             StatusToMojom(status.value));
              },
          .disconnected_cb =
              [remote](const std::string& endpoint_id) {
                if (!remote)
                  return;

                remote->OnDisconnected(endpoint_id);
              },
          .bandwidth_changed_cb =
              [remote](const std::string& endpoint_id, Medium medium) {
                if (!remote)
                  return;

                remote->OnBandwidthChanged(endpoint_id, MediumToMojom(medium));
              },
      },
  };
}

}  // namespace

// Should only be accessed by objects within lifetime of NearbyConnections.
NearbyConnections* g_instance = nullptr;

// static
NearbyConnections& NearbyConnections::GetInstance() {
  DCHECK(g_instance);
  return *g_instance;
}

NearbyConnections::NearbyConnections(
    mojo::PendingReceiver<mojom::NearbyConnections> nearby_connections,
    mojom::NearbyConnectionsDependenciesPtr dependencies,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    base::OnceClosure on_disconnect)
    : nearby_connections_(this, std::move(nearby_connections)),
      on_disconnect_(std::move(on_disconnect)),
      thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  location::nearby::api::LogMessage::SetMinLogSeverity(
      dependencies->min_log_severity);

  nearby_connections_.set_disconnect_handler(base::BindOnce(
      &NearbyConnections::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
      MojoDependencyName::kNearbyConnections));

  if (dependencies->bluetooth_adapter) {
    bluetooth_adapter_.Bind(std::move(dependencies->bluetooth_adapter),
                            io_task_runner);
    bluetooth_adapter_.set_disconnect_handler(
        base::BindOnce(&NearbyConnections::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr(),
                       MojoDependencyName::kBluetoothAdapter),
        base::SequencedTaskRunnerHandle::Get());
  }

  socket_manager_.Bind(
      std::move(dependencies->webrtc_dependencies->socket_manager),
      io_task_runner);
  socket_manager_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kSocketManager),
      base::SequencedTaskRunnerHandle::Get());

  mdns_responder_factory_.Bind(
      std::move(dependencies->webrtc_dependencies->mdns_responder_factory),
      io_task_runner);
  mdns_responder_factory_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kMdnsResponder),
      base::SequencedTaskRunnerHandle::Get());

  ice_config_fetcher_.Bind(
      std::move(dependencies->webrtc_dependencies->ice_config_fetcher),
      io_task_runner);
  ice_config_fetcher_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kIceConfigFetcher),
      base::SequencedTaskRunnerHandle::Get());

  webrtc_signaling_messenger_.Bind(
      std::move(dependencies->webrtc_dependencies->messenger), io_task_runner);
  webrtc_signaling_messenger_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kWebRtcSignalingMessenger),
      base::SequencedTaskRunnerHandle::Get());

  // There should only be one instance of NearbyConnections in a process.
  DCHECK(!g_instance);
  g_instance = this;
}

NearbyConnections::~NearbyConnections() {
  // We need to clean up the shared OfflineServiceController before cleaning up
  // Core objects. This ensures that any tasks queued up on threads get run
  // before the ClientProxy owned by Core is deleted. The ServiceControllerProxy
  // for each Core uses a reference to the unique_ptr so it will understand that
  // it can no longer forward calls once it is reset here.
  // See http://b/177336457 and https://crbug.com/1149773 for more details.

  // We call StopAllEndpoints() for each Core which is the same as
  // ClientDisconnecting() to simulate what happens when the
  // ServiceControllerRouter shuts down.
  CountDownLatch latch(service_id_to_core_map_.size());
  for (auto& pair : service_id_to_core_map_) {
    pair.second->StopAllEndpoints(
        {.result_cb = [&latch](Status status) { latch.CountDown(); }});
  }
  VLOG(1) << "Nearby Connections: waiting for Core objects to finish stopping "
          << "all endpoints.";
  if (!latch.Await(absl::Seconds(5)).result()) {
    LOG(FATAL) << __func__ << ": Failed to stop all endpoints on each Core in "
               << "time. Look for deadlocks in the threads tab of this crash.";
  }

  VLOG(1) << "Nearby Connections: shutting down the shared service controller "
          << "prior to taking down Core objects";
  service_controller_.reset();

  // Note that deleting active Core objects invokes their shutdown flows. This
  // is required to ensure that Nearby cleans itself up.
  VLOG(1) << "Nearby Connections: cleaning up Core objects";
  service_id_to_core_map_.clear();
  g_instance = nullptr;

  VLOG(1) << "Nearby Connections: shutdown complete";
}

std::string NearbyConnections::GetMojoDependencyName(
    MojoDependencyName dependency_name) {
  switch (dependency_name) {
    case MojoDependencyName::kNearbyConnections:
      return "Nearby Connections";
    case MojoDependencyName::kBluetoothAdapter:
      return "Bluetooth Adapter";
    case MojoDependencyName::kSocketManager:
      return "Socket Manager";
    case MojoDependencyName::kMdnsResponder:
      return "MDNS Responder";
    case MojoDependencyName::kIceConfigFetcher:
      return "ICE Config Fetcher";
    case MojoDependencyName::kWebRtcSignalingMessenger:
      return "WebRTC Signaling Messenger";
  }
}

void NearbyConnections::OnDisconnect(MojoDependencyName dependency_name) {
  if (!on_disconnect_) {
    return;
  }

  LOG(WARNING) << "Nearby dependency mojo disconnected: ["
               << GetMojoDependencyName(dependency_name) << "]";
  base::UmaHistogramEnumeration(
      "Nearby.Connections.UtilityProcessShutdownReason."
      "DisconnectedMojoDependency",
      dependency_name);

  std::move(on_disconnect_).Run();
  // Note: |this| might be destroyed here.
}

void NearbyConnections::StartAdvertising(
    const std::string& service_id,
    const std::vector<uint8_t>& endpoint_info,
    mojom::AdvertisingOptionsPtr options,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
    StartAdvertisingCallback callback) {
  ConnectionOptions connection_options{
      .strategy = StrategyFromMojom(options->strategy),
      .allowed = MediumSelectorFromMojom(options->allowed_mediums.get()),
      .auto_upgrade_bandwidth = options->auto_upgrade_bandwidth,
      .enforce_topology_constraints = options->enforce_topology_constraints,
      .enable_bluetooth_listening = options->enable_bluetooth_listening,
      .enable_webrtc_listening = options->enable_webrtc_listening,
      .fast_advertisement_service_uuid =
          options->fast_advertisement_service_uuid.canonical_value()};

  GetCore(service_id)
      ->StartAdvertising(
          service_id, std::move(connection_options),
          CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
          ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StopAdvertising(const std::string& service_id,
                                        StopAdvertisingCallback callback) {
  GetCore(service_id)
      ->StopAdvertising(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StartDiscovery(
    const std::string& service_id,
    mojom::DiscoveryOptionsPtr options,
    mojo::PendingRemote<mojom::EndpointDiscoveryListener> listener,
    StartDiscoveryCallback callback) {
  // Left as empty string if no value has been passed in |options|.
  std::string fast_advertisement_service_uuid;
  if (options->fast_advertisement_service_uuid) {
    fast_advertisement_service_uuid =
        options->fast_advertisement_service_uuid->canonical_value();
  }

  ConnectionOptions connection_options{
      .strategy = StrategyFromMojom(options->strategy),
      .allowed = MediumSelectorFromMojom(options->allowed_mediums.get()),
      .is_out_of_band_connection = options->is_out_of_band_connection,
      .fast_advertisement_service_uuid = fast_advertisement_service_uuid};
  mojo::SharedRemote<mojom::EndpointDiscoveryListener> remote(
      std::move(listener), thread_task_runner_);
  DiscoveryListener discovery_listener{
      .endpoint_found_cb =
          [task_runner = thread_task_runner_, remote](
              const std::string& endpoint_id, const ByteArray& endpoint_info,
              const std::string& service_id) {
            if (!remote) {
              return;
            }

            // This call must be posted to the same sequence that |remote| was
            // bound on.
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &mojom::EndpointDiscoveryListener::OnEndpointFound,
                    base::Unretained(remote.get()), endpoint_id,
                    mojom::DiscoveredEndpointInfo::New(
                        ByteArrayToMojom(endpoint_info), service_id)));
          },
      .endpoint_lost_cb =
          [task_runner = thread_task_runner_,
           remote](const std::string& endpoint_id) {
            if (!remote) {
              return;
            }

            // This call must be posted to the same sequence that |remote| was
            // bound on.
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &mojom::EndpointDiscoveryListener::OnEndpointLost,
                    base::Unretained(remote.get()), endpoint_id));
          },
  };
  ResultCallback result_callback = ResultCallbackFromMojom(std::move(callback));

  GetCore(service_id)
      ->StartDiscovery(service_id, std::move(connection_options),
                       std::move(discovery_listener),
                       std::move(result_callback));
}

void NearbyConnections::StopDiscovery(const std::string& service_id,
                                      StopDiscoveryCallback callback) {
  GetCore(service_id)
      ->StopDiscovery(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::InjectBluetoothEndpoint(
    const std::string& service_id,
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    const std::vector<uint8_t>& remote_bluetooth_mac_address,
    InjectBluetoothEndpointCallback callback) {
  OutOfBandConnectionMetadata oob_metadata{
      .medium = Medium::BLUETOOTH,
      .endpoint_id = endpoint_id,
      .endpoint_info = ByteArrayFromMojom(endpoint_info),
      .remote_bluetooth_mac_address =
          ByteArrayFromMojom(remote_bluetooth_mac_address)};
  GetCore(service_id)
      ->InjectEndpoint(service_id, oob_metadata,
                       ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RequestConnection(
    const std::string& service_id,
    const std::vector<uint8_t>& endpoint_info,
    const std::string& endpoint_id,
    mojom::ConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
    RequestConnectionCallback callback) {
  ConnectionOptions connection_options{
      .allowed = MediumSelectorFromMojom(options->allowed_mediums.get())};
  if (options->remote_bluetooth_mac_address) {
    connection_options.remote_bluetooth_mac_address =
        ByteArrayFromMojom(*options->remote_bluetooth_mac_address);
  }
  GetCore(service_id)
      ->RequestConnection(
          endpoint_id,
          CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
          std::move(connection_options),
          ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::DisconnectFromEndpoint(
    const std::string& service_id,
    const std::string& endpoint_id,
    DisconnectFromEndpointCallback callback) {
  GetCore(service_id)
      ->DisconnectFromEndpoint(endpoint_id,
                               ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::AcceptConnection(
    const std::string& service_id,
    const std::string& endpoint_id,
    mojo::PendingRemote<mojom::PayloadListener> listener,
    AcceptConnectionCallback callback) {
  mojo::SharedRemote<mojom::PayloadListener> remote(std::move(listener));
  // Capturing Core* is safe as Core owns PayloadListener.
  PayloadListener payload_listener = {
      .payload_cb =
          [&, remote, core = GetCore(service_id)](
              const std::string& endpoint_id, Payload payload) {
            if (!remote)
              return;

            switch (payload.GetType()) {
              case Payload::Type::kBytes: {
                mojom::BytesPayloadPtr bytes_payload = mojom::BytesPayload::New(
                    ByteArrayToMojom(payload.AsBytes()));
                remote->OnPayloadReceived(
                    endpoint_id,
                    mojom::Payload::New(payload.GetId(),
                                        mojom::PayloadContent::NewBytes(
                                            std::move(bytes_payload))));
                break;
              }
              case Payload::Type::kFile: {
                DCHECK(payload.AsFile());
                // InputFile is created by Chrome, so it's safe to downcast.
                chrome::InputFile& input_file = static_cast<chrome::InputFile&>(
                    payload.AsFile()->GetInputStream());
                base::File file = input_file.ExtractUnderlyingFile();
                if (!file.IsValid()) {
                  core->CancelPayload(payload.GetId(), /*callback=*/{});
                  return;
                }

                mojom::FilePayloadPtr file_payload =
                    mojom::FilePayload::New(std::move(file));
                remote->OnPayloadReceived(
                    endpoint_id,
                    mojom::Payload::New(payload.GetId(),
                                        mojom::PayloadContent::NewFile(
                                            std::move(file_payload))));
                break;
              }
              case Payload::Type::kStream:
                buffer_manager_.StartTrackingPayload(std::move(payload));
                break;
              case Payload::Type::kUnknown:
                core->CancelPayload(payload.GetId(), /*callback=*/{});
                return;
            }
          },
      .payload_progress_cb =
          [&, remote](const std::string& endpoint_id,
                      const PayloadProgressInfo& info) {
            if (!remote)
              return;

            DCHECK_GE(info.total_bytes, 0);
            DCHECK_GE(info.bytes_transferred, 0);
            remote->OnPayloadTransferUpdate(
                endpoint_id,
                mojom::PayloadTransferUpdate::New(
                    info.payload_id, PayloadStatusToMojom(info.status),
                    info.total_bytes, info.bytes_transferred));

            if (!buffer_manager_.IsTrackingPayload(info.payload_id))
              return;

            switch (info.status) {
              case PayloadProgressInfo::Status::kFailure:
                FALLTHROUGH;
              case PayloadProgressInfo::Status::kCanceled:
                buffer_manager_.StopTrackingFailedPayload(info.payload_id);
                break;

              case PayloadProgressInfo::Status::kInProgress:
                // Note that |info.bytes_transferred| is a cumulative measure of
                // bytes that have been sent so far in the payload.
                buffer_manager_.HandleBytesTransferred(info.payload_id,
                                                       info.bytes_transferred);
                break;

              case PayloadProgressInfo::Status::kSuccess:
                // When kSuccess is passed, we are guaranteed to have received a
                // previous kInProgress update with the same |bytes_transferred|
                // value.
                // Since we have completed fetching the full payload, return the
                // completed payload as a "bytes" payload.
                remote->OnPayloadReceived(
                    endpoint_id,
                    mojom::Payload::New(
                        info.payload_id,
                        mojom::PayloadContent::NewBytes(
                            mojom::BytesPayload::New(ByteArrayToMojom(
                                buffer_manager_
                                    .GetCompletePayloadAndStopTracking(
                                        info.payload_id))))));
                break;
            }
          }};

  GetCore(service_id)
      ->AcceptConnection(endpoint_id, std::move(payload_listener),
                         ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RejectConnection(const std::string& service_id,
                                         const std::string& endpoint_id,
                                         RejectConnectionCallback callback) {
  GetCore(service_id)
      ->RejectConnection(endpoint_id,
                         ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::SendPayload(
    const std::string& service_id,
    const std::vector<std::string>& endpoint_ids,
    mojom::PayloadPtr payload,
    SendPayloadCallback callback) {
  Payload core_payload;
  switch (payload->content->which()) {
    case mojom::PayloadContent::Tag::BYTES:
      core_payload =
          Payload(payload->id,
                  ByteArrayFromMojom(payload->content->get_bytes()->bytes));
      break;
    case mojom::PayloadContent::Tag::FILE:
      int64_t file_size = payload->content->get_file()->file.GetLength();
      {
        base::AutoLock al(input_file_lock_);
        input_file_map_.insert_or_assign(
            payload->id, std::move(payload->content->get_file()->file));
      }
      core_payload = Payload(payload->id, InputFile(payload->id, file_size));
      break;
  }

  GetCore(service_id)
      ->SendPayload(absl::MakeSpan(endpoint_ids), std::move(core_payload),
                    ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::CancelPayload(const std::string& service_id,
                                      int64_t payload_id,
                                      CancelPayloadCallback callback) {
  GetCore(service_id)
      ->CancelPayload(payload_id, ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StopAllEndpoints(const std::string& service_id,
                                         StopAllEndpointsCallback callback) {
  GetCore(service_id)
      ->StopAllEndpoints(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::InitiateBandwidthUpgrade(
    const std::string& service_id,
    const std::string& endpoint_id,
    InitiateBandwidthUpgradeCallback callback) {
  GetCore(service_id)
      ->InitiateBandwidthUpgrade(endpoint_id,
                                 ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RegisterPayloadFile(
    const std::string& service_id,
    int64_t payload_id,
    base::File input_file,
    base::File output_file,
    RegisterPayloadFileCallback callback) {
  if (!input_file.IsValid() || !output_file.IsValid()) {
    std::move(callback).Run(mojom::Status::kError);
    return;
  }

  {
    base::AutoLock al(input_file_lock_);
    input_file_map_.insert_or_assign(payload_id, std::move(input_file));
  }

  {
    base::AutoLock al(output_file_lock_);
    output_file_map_.insert_or_assign(payload_id, std::move(output_file));
  }

  std::move(callback).Run(mojom::Status::kSuccess);
}

base::File NearbyConnections::ExtractInputFile(int64_t payload_id) {
  base::AutoLock al(input_file_lock_);
  auto file_it = input_file_map_.find(payload_id);
  if (file_it == input_file_map_.end())
    return base::File();

  base::File file = std::move(file_it->second);
  input_file_map_.erase(file_it);
  return file;
}

base::File NearbyConnections::ExtractOutputFile(int64_t payload_id) {
  base::AutoLock al(output_file_lock_);
  auto file_it = output_file_map_.find(payload_id);
  if (file_it == output_file_map_.end())
    return base::File();

  base::File file = std::move(file_it->second);
  output_file_map_.erase(file_it);
  return file;
}

scoped_refptr<base::SingleThreadTaskRunner>
NearbyConnections::GetThreadTaskRunner() {
  return thread_task_runner_;
}

Core* NearbyConnections::GetCore(const std::string& service_id) {
  std::unique_ptr<Core>& core = service_id_to_core_map_[service_id];

  if (!core) {
    // Note: Some tests will use SetServiceControllerForTesting to set a
    // |service_controller| instance, but this value is expected to be null for
    // the first GetCore() call during normal operation.
    if (!service_controller_) {
      service_controller_ = std::make_unique<OfflineServiceController>();
    }

    core = std::make_unique<Core>([&]() {
      // Core expects to take ownership of the pointer provided, but since we
      // share a single ServiceController among all Core objects created, we
      // provide a proxy which calls into our shared instance.
      // The |service_controller_| is passed by reference to the unique_ptr so
      // the proxy knows if |service_controller_| has been reset.
      return new ServiceControllerProxy(service_controller_);
    });
  }

  return core.get();
}

void NearbyConnections::SetServiceControllerForTesting(
    std::unique_ptr<ServiceController> service_controller) {
  service_controller_ = std::move(service_controller);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
