// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

#include "base/files/file_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/services/sharing/nearby/nearby_connections_conversions.h"
#include "chrome/services/sharing/nearby/platform_v2/input_file.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"

namespace location {
namespace nearby {
namespace connections {

namespace {

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
    base::OnceClosure on_disconnect,
    std::unique_ptr<Core> core)
    : nearby_connections_(this, std::move(nearby_connections)),
      on_disconnect_(std::move(on_disconnect)),
      core_(std::move(core)),
      thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  nearby_connections_.set_disconnect_handler(base::BindOnce(
      &NearbyConnections::OnDisconnect, weak_ptr_factory_.GetWeakPtr()));

  if (dependencies->bluetooth_adapter) {
    bluetooth_adapter_.Bind(std::move(dependencies->bluetooth_adapter),
                            io_task_runner);
    bluetooth_adapter_.set_disconnect_handler(
        base::BindOnce(&NearbyConnections::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr()),
        base::SequencedTaskRunnerHandle::Get());
  }

  socket_manager_.Bind(
      std::move(dependencies->webrtc_dependencies->socket_manager),
      io_task_runner);
  socket_manager_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  mdns_responder_.Bind(
      std::move(dependencies->webrtc_dependencies->mdns_responder),
      io_task_runner);
  mdns_responder_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  ice_config_fetcher_.Bind(
      std::move(dependencies->webrtc_dependencies->ice_config_fetcher),
      io_task_runner);
  ice_config_fetcher_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  webrtc_signaling_messenger_.Bind(
      std::move(dependencies->webrtc_dependencies->messenger), io_task_runner);
  webrtc_signaling_messenger_.set_disconnect_handler(
      base::BindOnce(&NearbyConnections::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  // There should only be one instance of NearbyConnections in a process.
  DCHECK(!g_instance);
  g_instance = this;
}

NearbyConnections::~NearbyConnections() {
  core_.reset();
  g_instance = nullptr;
}

void NearbyConnections::OnDisconnect() {
  if (on_disconnect_)
    std::move(on_disconnect_).Run();
  // Note: |this| might be destroyed here.
}

void NearbyConnections::StartAdvertising(
    const std::vector<uint8_t>& endpoint_info,
    const std::string& service_id,
    mojom::AdvertisingOptionsPtr options,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
    StartAdvertisingCallback callback) {
  ConnectionOptions connection_options{
      .strategy = StrategyFromMojom(options->strategy),
      .allowed = MediumSelectorFromMojom(options->allowed_mediums.get()),
      .auto_upgrade_bandwidth = options->auto_upgrade_bandwidth,
      .enforce_topology_constraints = options->enforce_topology_constraints,
      .enable_bluetooth_listening = options->enable_bluetooth_listening,
      .fast_advertisement_service_uuid =
          options->fast_advertisement_service_uuid.canonical_value()};

  core_->StartAdvertising(
      service_id, std::move(connection_options),
      CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
      ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StopAdvertising(StopAdvertisingCallback callback) {
  core_->StopAdvertising(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StartDiscovery(
    const std::string& service_id,
    mojom::DiscoveryOptionsPtr options,
    mojo::PendingRemote<mojom::EndpointDiscoveryListener> listener,
    StartDiscoveryCallback callback) {
  ConnectionOptions connection_options{
      .strategy = StrategyFromMojom(options->strategy)};
  mojo::SharedRemote<mojom::EndpointDiscoveryListener> remote(
      std::move(listener));
  DiscoveryListener discovery_listener{
      .endpoint_found_cb =
          [remote](const std::string& endpoint_id,
                   const ByteArray& endpoint_info,
                   const std::string& service_id) {
            if (!remote) {
              return;
            }

            remote->OnEndpointFound(
                endpoint_id, mojom::DiscoveredEndpointInfo::New(
                                 ByteArrayToMojom(endpoint_info), service_id));
          },
      .endpoint_lost_cb =
          [remote](const std::string& endpoint_id) {
            if (!remote)
              return;

            remote->OnEndpointLost(endpoint_id);
          },
  };
  ResultCallback result_callback = ResultCallbackFromMojom(std::move(callback));

  core_->StartDiscovery(service_id, std::move(connection_options),
                        std::move(discovery_listener),
                        std::move(result_callback));
}

void NearbyConnections::StopDiscovery(StopDiscoveryCallback callback) {
  core_->StopDiscovery(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RequestConnection(
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
  core_->RequestConnection(
      endpoint_id,
      CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
      std::move(connection_options),
      ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::DisconnectFromEndpoint(
    const std::string& endpoint_id,
    DisconnectFromEndpointCallback callback) {
  core_->DisconnectFromEndpoint(endpoint_id,
                                ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::AcceptConnection(
    const std::string& endpoint_id,
    mojo::PendingRemote<mojom::PayloadListener> listener,
    AcceptConnectionCallback callback) {
  mojo::SharedRemote<mojom::PayloadListener> remote(std::move(listener));
  // Capturing Core* is safe as Core owns PayloadListener.
  PayloadListener payload_listener = {
      .payload_cb =
          [remote, core = core_.get()](const std::string& endpoint_id,
                                       Payload payload) {
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
                // Stream payload is not supported.
              case Payload::Type::kUnknown:
                core->CancelPayload(payload.GetId(), /*callback=*/{});
                return;
            }
          },
      .payload_progress_cb =
          [remote](const std::string& endpoint_id,
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
          }};

  core_->AcceptConnection(endpoint_id, std::move(payload_listener),
                          ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RejectConnection(const std::string& endpoint_id,
                                         RejectConnectionCallback callback) {
  core_->RejectConnection(endpoint_id,
                          ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::SendPayload(
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

  core_->SendPayload(absl::MakeSpan(endpoint_ids), std::move(core_payload),
                     ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::CancelPayload(int64_t payload_id,
                                      CancelPayloadCallback callback) {
  core_->CancelPayload(payload_id,
                       ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StopAllEndpoints(StopAllEndpointsCallback callback) {
  core_->StopAllEndpoints(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::InitiateBandwidthUpgrade(
    const std::string& endpoint_id,
    InitiateBandwidthUpgradeCallback callback) {
  core_->InitiateBandwidthUpgrade(endpoint_id,
                                  ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RegisterPayloadFile(
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

}  // namespace connections
}  // namespace nearby
}  // namespace location
