// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_connection.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "chromeos/services/secure_channel/wire_message.h"

namespace chromeos {

namespace secure_channel {

FakeConnection::FakeConnection(multidevice::RemoteDeviceRef remote_device)
    : FakeConnection(remote_device, /* should_auto_connect */ true) {}

FakeConnection::FakeConnection(multidevice::RemoteDeviceRef remote_device,
                               bool should_auto_connect)
    : Connection(remote_device), should_auto_connect_(should_auto_connect) {
  if (should_auto_connect_) {
    Connect();
  }
}

FakeConnection::~FakeConnection() {
  Disconnect();
}

void FakeConnection::Connect() {
  if (should_auto_connect_) {
    SetStatus(Status::CONNECTED);
  } else {
    SetStatus(Status::IN_PROGRESS);
  }
}

void FakeConnection::Disconnect() {
  SetStatus(Status::DISCONNECTED);
}

std::string FakeConnection::GetDeviceAddress() {
  return std::string();
}

void FakeConnection::AddObserver(ConnectionObserver* observer) {
  observers_.push_back(observer);
  Connection::AddObserver(observer);
}

void FakeConnection::RemoveObserver(ConnectionObserver* observer) {
  base::Erase(observers_, observer);
  Connection::RemoveObserver(observer);
}

void FakeConnection::GetConnectionRssi(
    base::OnceCallback<void(absl::optional<int32_t>)> callback) {
  std::move(callback).Run(rssi_to_return_);
}

void FakeConnection::CompleteInProgressConnection(bool success) {
  DCHECK(!should_auto_connect_);
  DCHECK(status() == Status::IN_PROGRESS);

  if (success) {
    SetStatus(Status::CONNECTED);
  } else {
    SetStatus(Status::DISCONNECTED);
  }
}

void FakeConnection::FinishSendingMessageWithSuccess(bool success) {
  CHECK(current_message_);
  // Capture a copy of the message, as OnDidSendMessage() might reentrantly
  // call SendMessage().
  std::unique_ptr<WireMessage> sent_message = std::move(current_message_);
  OnDidSendMessage(*sent_message, success);
}

void FakeConnection::ReceiveMessage(const std::string& feature,
                                    const std::string& payload) {
  pending_feature_ = feature;
  pending_payload_ = payload;
  OnBytesReceived(std::string());
  pending_feature_.clear();
  pending_payload_.clear();
}

void FakeConnection::SendMessageImpl(std::unique_ptr<WireMessage> message) {
  CHECK(!current_message_);
  current_message_ = std::move(message);
}

std::unique_ptr<WireMessage> FakeConnection::DeserializeWireMessage(
    bool* is_incomplete_message) {
  *is_incomplete_message = false;
  return std::make_unique<WireMessage>(pending_payload_, pending_feature_);
}

}  // namespace secure_channel

}  // namespace chromeos
