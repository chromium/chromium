// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_secure_channel_connection.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"

namespace ash::secure_channel {

FakeSecureChannelConnection::SentMessage::SentMessage(
    const std::string& feature,
    const std::string& payload)
    : feature(feature), payload(payload) {}

FakeSecureChannelConnection::FakeSecureChannelConnection(
    std::unique_ptr<Connection> connection)
    : SecureChannel(std::move(connection)) {}

FakeSecureChannelConnection::~FakeSecureChannelConnection() {
  if (destructor_callback_)
    std::move(destructor_callback_).Run();
}

void FakeSecureChannelConnection::ChangeStatus(const Status& new_status) {
  Status old_status = status_;
  status_ = new_status;

  // Copy to prevent channel from being removed during handler.
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_copy =
      observers_;
  for (ash::secure_channel::SecureChannel::Observer* observer :
       observers_copy) {
    observer->OnSecureChannelStatusChanged(this, old_status, status_);
  }
}

void FakeSecureChannelConnection::ChangeNearbyConnectionState(
    mojom::NearbyConnectionStep nearby_connection_step,
    mojom::NearbyConnectionStepResult result) {
  // Copy to prevent channel from being removed during handler.
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_copy =
      observers_;
  for (Observer* observer : observers_copy) {
    observer->OnNearbyConnectionStateChanged(this, nearby_connection_step,
                                             result);
  }
}

void FakeSecureChannelConnection::ChangeSecureChannelAuthenticationState(
    mojom::SecureChannelState secure_channel_authentication_state) {
  // Copy to prevent channel from being removed during handler.
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_copy =
      observers_;
  for (Observer* observer : observers_copy) {
    observer->OnSecureChannelAuthenticationStateChanged(
        this, secure_channel_authentication_state);
  }
}

void FakeSecureChannelConnection::ReceiveMessage(const std::string& feature,
                                                 const std::string& payload) {
  // Copy to prevent channel from being removed during handler.
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_copy =
      observers_;
  for (ash::secure_channel::SecureChannel::Observer* observer :
       observers_copy) {
    observer->OnMessageReceived(this, feature, payload);
  }
}

void FakeSecureChannelConnection::CompleteSendingMessage(int sequence_number) {
  DCHECK(next_sequence_number_ > sequence_number);
  // Copy to prevent channel from being removed during handler.
  std::vector<raw_ptr<Observer, VectorExperimental>> observers_copy =
      observers_;
  for (ash::secure_channel::SecureChannel::Observer* observer :
       observers_copy) {
    observer->OnMessageSent(this, sequence_number);
  }
}

void FakeSecureChannelConnection::Initialize() {
  was_initialized_ = true;
  ChangeStatus(Status::CONNECTING);
}

int FakeSecureChannelConnection::SendMessage(const std::string& feature,
                                             const std::string& payload) {
  sent_messages_.push_back(SentMessage(feature, payload));
  return next_sequence_number_++;
}

void FakeSecureChannelConnection::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  register_payload_file_requests_.emplace_back(
      payload_id, std::move(file_transfer_update_callback));
  std::move(registration_result_callback).Run(/*success=*/true);
}

void FakeSecureChannelConnection::Disconnect() {
  if (status() == Status::DISCONNECTING || status() == Status::DISCONNECTED)
    return;

  if (status() == Status::CONNECTING)
    ChangeStatus(Status::DISCONNECTED);
  else
    ChangeStatus(Status::DISCONNECTING);
}

void FakeSecureChannelConnection::AddObserver(Observer* observer) {
  observers_.push_back(observer);
}

void FakeSecureChannelConnection::RemoveObserver(Observer* observer) {
  observers_.erase(base::ranges::find(observers_, observer), observers_.end());
}

void FakeSecureChannelConnection::GetConnectionRssi(
    base::OnceCallback<void(std::optional<int32_t>)> callback) {
  std::move(callback).Run(rssi_to_return_);
}

std::optional<std::string>
FakeSecureChannelConnection::GetChannelBindingData() {
  return channel_binding_data_;
}

}  // namespace ash::secure_channel
