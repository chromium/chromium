// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/testing/fake_nearby_connections.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/files/file_util.h"
#include "base/notimplemented.h"
#include "base/rand_util.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_migration {
namespace {
constexpr std::string_view kTestAuthToken = "test-auth-token";
}  // namespace

FakeNearbyConnections::RegisteredFilePayload::RegisteredFilePayload() = default;

FakeNearbyConnections::RegisteredFilePayload::RegisteredFilePayload(
    base::File input_file_in,
    base::File output_file_in)
    : input_file(std::move(input_file_in)),
      output_file(std::move(output_file_in)) {}

FakeNearbyConnections::RegisteredFilePayload::RegisteredFilePayload(
    RegisteredFilePayload&&) = default;

FakeNearbyConnections::RegisteredFilePayload&
FakeNearbyConnections::RegisteredFilePayload::operator=(
    RegisteredFilePayload&&) = default;

FakeNearbyConnections::RegisteredFilePayload::~RegisteredFilePayload() =
    default;

FakeNearbyConnections::FakeNearbyConnections(
    std::string_view remote_endpoint_id)
    : remote_endpoint_id_(remote_endpoint_id) {
  CHECK(!remote_endpoint_id_.empty());
}

FakeNearbyConnections::~FakeNearbyConnections() = default;

bool FakeNearbyConnections::SendFile(int64_t payload_id,
                                     std::vector<uint8_t>* transferred_bytes) {
  if (transferred_bytes) {
    transferred_bytes->clear();
  }
  if (!remote_to_local_payload_listener_.is_bound()) {
    LOG(ERROR) << "Payload listener not bound. Cannot send file yet.";
    return false;
  }
  auto registered_files_iter = registered_files_.find(payload_id);
  if (registered_files_iter == registered_files_.end()) {
    LOG(ERROR) << "Payload id " << payload_id
               << " and its corresponding path not registered yet.";
    return false;
  }

  RegisteredFilePayload file_handles = std::move(registered_files_iter->second);
  registered_files_.erase(registered_files_iter);
  remote_to_local_payload_listener_->OnPayloadReceived(
      remote_endpoint_id_,
      ::nearby::connections::mojom::Payload::New(
          payload_id, ::nearby::connections::mojom::PayloadContent::NewFile(
                          ::nearby::connections::mojom::FilePayload::New(
                              std::move(file_handles.input_file)))));

  // For a successful case, break the file into 4 equal size chunks. For any
  // failure case, transfer one chunk of the file and then fail.
  PayloadStatus final_status = final_file_payload_status_.contains(payload_id)
                                   ? final_file_payload_status_.at(payload_id)
                                   : PayloadStatus::kSuccess;
  size_t num_chunks_to_transfer =
      final_status == PayloadStatus::kSuccess ? test_file_num_chunks_ : 1;
  size_t total_bytes_transferred = 0;
  for (size_t chunk_idx = 0; chunk_idx < num_chunks_to_transfer; ++chunk_idx) {
    const std::vector<uint8_t> new_chunk = base::RandBytesAsVector(
        test_file_size_in_bytes_ / test_file_num_chunks_);
    CHECK(!new_chunk.empty());
    base::File& output_file = file_handles.output_file;
    if (!output_file.WriteAtCurrentPosAndCheck(new_chunk) ||
        !output_file.Flush()) {
      LOG(ERROR) << "Failed to write file chunk for payload " << payload_id;
      return false;
    }

    if (transferred_bytes) {
      base::Extend(*transferred_bytes, new_chunk);
    }
    total_bytes_transferred += new_chunk.size();
    remote_to_local_payload_listener_->OnPayloadTransferUpdate(
        remote_endpoint_id_,
        ::nearby::connections::mojom::PayloadTransferUpdate::New(
            payload_id, PayloadStatus::kInProgress, test_file_size_in_bytes_,
            total_bytes_transferred));
  }
  remote_to_local_payload_listener_->OnPayloadTransferUpdate(
      remote_endpoint_id_,
      ::nearby::connections::mojom::PayloadTransferUpdate::New(
          payload_id, final_status, test_file_size_in_bytes_,
          total_bytes_transferred));
  return true;
}

void FakeNearbyConnections::SetFinalFilePayloadStatus(PayloadStatus status,
                                                      int64_t payload_id) {
  final_file_payload_status_[payload_id] = status;
}

bool FakeNearbyConnections::SendBytesPayload(int64_t payload_id,
                                             const std::string& bytes) {
  if (!remote_to_local_payload_listener_.is_bound()) {
    LOG(ERROR) << "Payload listener not bound. Cannot send bytes yet.";
    return false;
  }

  remote_to_local_payload_listener_->OnPayloadReceived(
      remote_endpoint_id_,
      ::nearby::connections::mojom::Payload::New(
          payload_id,
          ::nearby::connections::mojom::PayloadContent::NewBytes(
              ::nearby::connections::mojom::BytesPayload::New(
                  std::vector<uint8_t>(bytes.begin(), bytes.end())))));
  remote_to_local_payload_listener_->OnPayloadTransferUpdate(
      remote_endpoint_id_,
      ::nearby::connections::mojom::PayloadTransferUpdate::New(
          payload_id, PayloadStatus::kSuccess, bytes.size(), bytes.size()));
  return true;
}

bool FakeNearbyConnections::SimulateRemoteDisconnect() {
  if (!connection_listener_.is_bound()) {
    return false;
  }
  auto connection_listener = std::move(connection_listener_);
  DisconnectFromEndpoint(kServiceId, remote_endpoint_id_, base::DoNothing());
  connection_listener->OnDisconnected(remote_endpoint_id_);
  return true;
}

void FakeNearbyConnections::StartAdvertising(
    const std::string& service_id,
    const std::vector<uint8_t>& endpoint_info,
    ::nearby::connections::mojom::AdvertisingOptionsPtr options,
    mojo::PendingRemote<
        ::nearby::connections::mojom::ConnectionLifecycleListener> listener,
    StartAdvertisingCallback callback) {
  if (service_id != kServiceId) {
    GTEST_FAIL() << "StartAdvertising() call invalid. service_id="
                 << service_id;
  }

  if (is_advertising_) {
    std::move(callback).Run(Status::kAlreadyAdvertising);
    return;
  }

  is_advertising_ = true;
  connection_listener_.reset();
  connection_listener_.Bind(std::move(listener));

  // 1) Advertising starts successfully.
  std::move(callback).Run(Status::kSuccess);
  // 2) Immediately notify the ChromeOS target device of a connection
  //    initiation. This simulates immediate discovery in the real world.
  //
  // These are essential options for data_migration to work. If they're not
  // set properly, the `listener` will not receive any incoming connections,
  // which reflects reality.
  if (options->strategy ==
          ::nearby::connections::mojom::Strategy::kP2pPointToPoint &&
      options->allowed_mediums->bluetooth) {
    connection_listener_->OnConnectionInitiated(
        remote_endpoint_id_,
        ::nearby::connections::mojom::ConnectionInfo::New(
            kTestAuthToken.data(),
            /*raw_authentication_token=*/base::RandBytesAsVector(64),
            /*endpoint_info=*/std::vector<uint8_t>(64, 0),
            /*is_incoming_connection=*/true));
  } else {
    GTEST_FAIL() << "Invalid advertising options. strategy="
                 << options->strategy
                 << " bluetooth=" << options->allowed_mediums->bluetooth;
  }
}

void FakeNearbyConnections::StopAdvertising(const std::string& service_id,
                                            StopAdvertisingCallback callback) {
  if (service_id != kServiceId || !is_advertising_) {
    GTEST_FAIL() << "StopAdvertising() call invalid. service_id=" << service_id
                 << " is_advertising_=" << is_advertising_;
  }
  is_advertising_ = false;
  std::move(callback).Run(Status::kSuccess);
}

void FakeNearbyConnections::StartDiscovery(
    const std::string& service_id,
    ::nearby::connections::mojom::DiscoveryOptionsPtr options,
    mojo::PendingRemote<::nearby::connections::mojom::EndpointDiscoveryListener>
        listener,
    StartDiscoveryCallback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::StopDiscovery(const std::string& service_id,
                                          StopDiscoveryCallback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::InjectBluetoothEndpoint(
    const std::string& service_id,
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    const std::vector<uint8_t>& remote_bluetooth_mac_address,
    InjectBluetoothEndpointCallback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::RequestConnection(
    const std::string& service_id,
    const std::vector<uint8_t>& endpoint_info,
    const std::string& endpoint_id,
    ::nearby::connections::mojom::ConnectionOptionsPtr options,
    mojo::PendingRemote<
        ::nearby::connections::mojom::ConnectionLifecycleListener> listener,
    RequestConnectionCallback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::DisconnectFromEndpoint(
    const std::string& service_id,
    const std::string& endpoint_id,
    DisconnectFromEndpointCallback callback) {
  if (service_id == kServiceId && endpoint_id == remote_endpoint_id_) {
    connection_listener_.reset();
    remote_to_local_payload_listener_.reset();
    registered_files_.clear();
  } else {
    GTEST_FAIL() << "DisconnectFromEndpoint() call invalid. service_id="
                 << service_id << " endpoint_id=" << endpoint_id;
  }
  std::move(callback).Run(Status::kSuccess);
}

void FakeNearbyConnections::AcceptConnection(
    const std::string& service_id,
    const std::string& endpoint_id,
    mojo::PendingRemote<::nearby::connections::mojom::PayloadListener> listener,
    AcceptConnectionCallback callback) {
  // `service_id != kServiceId` - This class never initiates a connection for
  // a service other than data migration, so accepting a connection before an
  // initiation is out of order.
  //
  // `!connection_listener_.is_bound()` - The ChromeOS target device tried to
  // accept a connection before it was discovered. Also out of order.
  if (service_id != kServiceId || !connection_listener_.is_bound()) {
    GTEST_FAIL() << "AcceptConnection() call invalid. service_id=" << service_id
                 << " connection_listener_=" << connection_listener_.is_bound();
  }

  if (remote_to_local_payload_listener_.is_bound()) {
    std::move(callback).Run(Status::kAlreadyConnectedToEndpoint);
    return;
  }

  remote_to_local_payload_listener_.Bind(std::move(listener));
  std::move(callback).Run(Status::kSuccess);
  // In reality, the user would be prompted with a visual pin at this point and
  // need to confirm the transfer on the remote device before moving on. For
  // tests, assume this passes and establish the connection immediately
  // (ChromeOS just sent the remote device an "accept connection", and now
  // the remote device sends an "accept connection" back).
  connection_listener_->OnConnectionAccepted(remote_endpoint_id_);

  if (connection_established_listener_) {
    connection_established_listener_.Run();
  }
}

void FakeNearbyConnections::RejectConnection(
    const std::string& service_id,
    const std::string& endpoint_id,
    RejectConnectionCallback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::SendPayload(
    const std::string& service_id,
    const std::vector<std::string>& endpoint_ids,
    ::nearby::connections::mojom::PayloadPtr payload,
    SendPayloadCallback callback) {
  if (service_id != kServiceId) {
    GTEST_FAIL() << "Sending payload to unexpected service_id " << service_id;
  }

  if (!base::Contains(endpoint_ids, remote_endpoint_id_)) {
    std::move(callback).Run(Status::kEndpointUnknown);
    return;
  }

  // `remote_to_local_payload_listener_` is only bound after both sides of the
  // connection have been accepted. Although `remote_to_local_payload_listener_`
  // is not used in this method, it reflects reality. The local device cannot
  // send a payload until the connection is formed.
  if (!remote_to_local_payload_listener_.is_bound()) {
    std::move(callback).Run(Status::kOutOfOrderApiCall);
    return;
  }

  if (local_to_remote_payload_listener_) {
    local_to_remote_payload_listener_.Run(std::move(payload));
  }
  std::move(callback).Run(Status::kSuccess);
}

void FakeNearbyConnections::CancelPayload(const std::string& service_id,
                                          int64_t payload_id,
                                          CancelPayloadCallback callback) {
  registered_files_.erase(payload_id);
  std::move(callback).Run(Status::kSuccess);
}

void FakeNearbyConnections::StopAllEndpoints(
    const std::string& service_id,
    StopAllEndpointsCallback callback) {
  DisconnectFromEndpoint(service_id, remote_endpoint_id_, std::move(callback));
}

void FakeNearbyConnections::InitiateBandwidthUpgrade(
    const std::string& service_id,
    const std::string& endpoint_id,
    InitiateBandwidthUpgradeCallback callback) {
  NOTIMPLEMENTED();
}

// This should happen before `FakeNearbyConnections::SendFile()`. This reflects
// the order of operations in reality.
void FakeNearbyConnections::RegisterPayloadFile(
    const std::string& service_id,
    int64_t payload_id,
    base::File input_file,
    base::File output_file,
    RegisterPayloadFileCallback callback) {
  if (service_id != kServiceId) {
    GTEST_FAIL() << "RegisterPayloadFile() call invalid. service_id="
                 << service_id;
  }

  Status result = Status::kSuccess;
  if (register_payload_file_result_generator_) {
    result = register_payload_file_result_generator_.Run();
  }

  if (result == Status::kSuccess) {
    registered_files_[payload_id] =
        RegisteredFilePayload(std::move(input_file), std::move(output_file));
  }
  std::move(callback).Run(result);
}

void FakeNearbyConnections::RequestConnectionV3(
    const std::string& service_id,
    ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
    ::nearby::connections::mojom::ConnectionOptionsPtr connection_options,
    mojo::PendingRemote<::nearby::connections::mojom::ConnectionListenerV3>
        listener,
    RequestConnectionV3Callback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::AcceptConnectionV3(
    const std::string& service_id,
    ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
    mojo::PendingRemote<::nearby::connections::mojom::PayloadListenerV3>
        listener,
    AcceptConnectionV3Callback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::RejectConnectionV3(
    const std::string& service_id,
    ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
    RejectConnectionV3Callback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::DisconnectFromDeviceV3(
    const std::string& service_id,
    ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
    DisconnectFromDeviceV3Callback callback) {
  NOTIMPLEMENTED();
}

void FakeNearbyConnections::RegisterServiceWithPresenceDeviceProvider(
    const std::string& service_id) {
  NOTIMPLEMENTED();
}

}  // namespace data_migration
