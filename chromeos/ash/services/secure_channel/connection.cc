// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/connection.h"

#include <sstream>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/secure_channel/connection_observer.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"

namespace ash::secure_channel {

Connection::Connection(multidevice::RemoteDeviceRef remote_device)
    : remote_device_(remote_device),
      status_(Status::DISCONNECTED),
      is_sending_message_(false) {}

Connection::~Connection() {}

bool Connection::IsConnected() const {
  return status_ == Status::CONNECTED;
}

void Connection::SendMessage(std::unique_ptr<WireMessage> message) {
  if (!IsConnected()) {
    PA_LOG(ERROR) << "Not yet connected; cannot send message to "
                  << GetDeviceInfoLogString();
    return;
  }

  if (is_sending_message_) {
    PA_LOG(ERROR) << "Cannot send message because another message is currently "
                  << "being sent to " << GetDeviceInfoLogString();
    return;
  }

  is_sending_message_ = true;
  SendMessageImpl(std::move(message));
}

void Connection::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  if (!IsConnected()) {
    PA_LOG(ERROR)
        << "Not yet connected; cannot register file payloads for device "
        << GetDeviceInfoLogString();
    std::move(registration_result_callback).Run(/*success=*/false);
    return;
  }

  RegisterPayloadFileImpl(payload_id, std::move(payload_files),
                          std::move(file_transfer_update_callback),
                          std::move(registration_result_callback));
}

void Connection::AddObserver(ConnectionObserver* observer) {
  observers_.AddObserver(observer);
}

void Connection::RemoveObserver(ConnectionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Connection::AddNearbyConnectionObserver(
    NearbyConnectionObserver* nearby_connection_observer) {
  nearby_connection_state_observers_.AddObserver(nearby_connection_observer);
}

void Connection::RemoveNearbyConnectionObserver(
    NearbyConnectionObserver* nearby_connection_observer) {
  nearby_connection_state_observers_.RemoveObserver(nearby_connection_observer);
}

void Connection::GetConnectionRssi(
    base::OnceCallback<void(std::optional<int32_t>)> callback) {
  std::move(callback).Run(std::nullopt);
}

void Connection::SetStatus(Status status) {
  if (status_ == status)
    return;

  received_bytes_.clear();

  Status old_status = status_;
  status_ = status;
  for (auto& observer : observers_)
    observer.OnConnectionStatusChanged(this, old_status, status_);
}

void Connection::SetNearbyConnectionSubStatus(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  for (auto& observer : nearby_connection_state_observers_) {
    observer.OnNearbyConnectionStateChagned(step, result);
  }
}

void Connection::OnDidSendMessage(const WireMessage& message, bool success) {
  if (!is_sending_message_) {
    PA_LOG(ERROR) << "OnDidSendMessage(), but a send was not expected to be in "
                  << "progress to " << GetDeviceInfoLogString();
    return;
  }

  is_sending_message_ = false;
  for (auto& observer : observers_)
    observer.OnSendCompleted(*this, message, success);
}

void Connection::OnBytesReceived(const std::string& bytes) {
  if (!IsConnected()) {
    PA_LOG(ERROR) << "OnBytesReceived(), but not connected to "
                  << GetDeviceInfoLogString();
    return;
  }

  received_bytes_ += bytes;

  bool is_incomplete_message;
  std::unique_ptr<WireMessage> message =
      DeserializeWireMessage(&is_incomplete_message);
  if (is_incomplete_message)
    return;

  if (message) {
    for (auto& observer : observers_)
      observer.OnMessageReceived(*this, *message);
  }

  // Whether the message was parsed successfully or not, clear the
  // |received_bytes_| buffer.
  received_bytes_.clear();
}

std::unique_ptr<WireMessage> Connection::DeserializeWireMessage(
    bool* is_incomplete_message) {
  return WireMessage::Deserialize(received_bytes_, is_incomplete_message);
}

std::string Connection::GetDeviceInfoLogString() {
  std::stringstream ss;
  ss << "{id: \"" << remote_device().GetTruncatedDeviceIdForLogs()
     << "\", addr: \"" << GetDeviceAddress() << "\"}";
  return ss.str();
}

std::ostream& operator<<(std::ostream& stream,
                         const Connection::Status& status) {
  switch (status) {
    case Connection::Status::DISCONNECTED:
      stream << "[disconnected]";
      break;
    case Connection::Status::IN_PROGRESS:
      stream << "[in progress]";
      break;
    case Connection::Status::CONNECTED:
      stream << "[connected]";
      break;
  }

  return stream;
}

}  // namespace ash::secure_channel
