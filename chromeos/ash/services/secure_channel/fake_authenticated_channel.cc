// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_authenticated_channel.h"

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"

namespace ash::secure_channel {

FakeAuthenticatedChannel::FakeAuthenticatedChannel() : AuthenticatedChannel() {}

FakeAuthenticatedChannel::~FakeAuthenticatedChannel() = default;

void FakeAuthenticatedChannel::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  return std::move(callback).Run(std::move(connection_metadata_for_next_call_));
}

void FakeAuthenticatedChannel::PerformSendMessage(
    const std::string& feature,
    const std::string& payload,
    base::OnceClosure on_sent_callback) {
  sent_messages_.push_back(
      std::make_tuple(feature, payload, std::move(on_sent_callback)));
}

void FakeAuthenticatedChannel::PerformRegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  reigster_payload_file_requests_.emplace_back(
      payload_id, std::move(file_transfer_update_callback));
  std::move(registration_result_callback).Run(/*success=*/true);
}

void FakeAuthenticatedChannel::PerformDisconnection() {
  has_disconnection_been_requested_ = true;
}

FakeAuthenticatedChannelObserver::FakeAuthenticatedChannelObserver() = default;

FakeAuthenticatedChannelObserver::~FakeAuthenticatedChannelObserver() = default;

void FakeAuthenticatedChannelObserver::OnDisconnected() {
  has_been_notified_of_disconnection_ = true;
}

void FakeAuthenticatedChannelObserver::OnMessageReceived(
    const std::string& feature,
    const std::string& payload) {
  received_messages_.push_back(std::make_pair(feature, payload));
}

void FakeAuthenticatedChannelObserver::OnNearbyConnectionStateChanged(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  nearby_connection_step_ = step;
  nearby_connection_step_result_ = result;
}

}  // namespace ash::secure_channel
