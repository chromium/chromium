// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_modem_messaging_client.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace ash {

FakeModemMessagingClient::FakeModemMessagingClient() = default;
FakeModemMessagingClient::~FakeModemMessagingClient() = default;

void FakeModemMessagingClient::SetSmsReceivedHandler(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const SmsReceivedHandler& handler) {
  sms_received_handlers_.insert(
      std::pair<dbus::ObjectPath, SmsReceivedHandler>(object_path, handler));
  message_paths_map_.insert(
      std::pair<dbus::ObjectPath, std::vector<dbus::ObjectPath>>(object_path,
                                                                 {}));
}

void FakeModemMessagingClient::ResetSmsReceivedHandler(
    const std::string& service_name,
    const dbus::ObjectPath& object_path) {
  sms_received_handlers_[object_path].Reset();
}

void FakeModemMessagingClient::Delete(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& sms_path,
    chromeos::VoidDBusMethodCallback callback) {
  DCHECK(!delete_request_.has_value());
  delete_request_ = DeleteRequest{object_path, sms_path, std::move(callback)};
}

void FakeModemMessagingClient::List(const std::string& service_name,
                                    const dbus::ObjectPath& object_path,
                                    ListCallback callback) {
  std::move(callback).Run(message_paths_map_[object_path]);
}

ModemMessagingClient::TestInterface*
FakeModemMessagingClient::GetTestInterface() {
  return this;
}

// ModemMessagingClient::TestInterface overrides.

void FakeModemMessagingClient::ReceiveSms(const dbus::ObjectPath& object_path,
                                          const dbus::ObjectPath& sms_path) {
  if (message_paths_map_.find(object_path) == message_paths_map_.end()) {
    NOTREACHED_IN_MIGRATION() << "object_path not found!";
    return;
  }

  message_paths_map_[object_path].push_back(sms_path);
  sms_received_handlers_[object_path].Run(sms_path, true);
}

std::string FakeModemMessagingClient::GetPendingDeleteRequestSmsPath() const {
  if (!delete_request_.has_value()) {
    return std::string();
  }
  return delete_request_->sms_path.value();
}

void FakeModemMessagingClient::CompletePendingDeleteRequest(bool success) {
  DCHECK(delete_request_.has_value());
  DeleteRequest delete_request = std::move(delete_request_.value());
  delete_request_.reset();
  if (!success) {
    std::move(delete_request.callback).Run(false);
    return;
  }

  std::vector<dbus::ObjectPath> message_paths =
      message_paths_map_[delete_request.object_path];
  auto iter =
      find(message_paths.begin(), message_paths.end(), delete_request.sms_path);
  if (iter != message_paths.end()) {
    message_paths.erase(iter);
  }
  std::move(delete_request.callback).Run(true);
}

FakeModemMessagingClient::DeleteRequest::DeleteRequest(
    dbus::ObjectPath object_path_param,
    dbus::ObjectPath sms_path_param,
    chromeos::VoidDBusMethodCallback callback_param)
    : object_path(object_path_param),
      sms_path(sms_path_param),
      callback(std::move(callback_param)) {}

FakeModemMessagingClient::DeleteRequest::DeleteRequest(DeleteRequest&& other) {
  object_path = other.object_path;
  sms_path = other.sms_path;
  callback = std::move(other.callback);
}

FakeModemMessagingClient::DeleteRequest&
FakeModemMessagingClient::DeleteRequest::operator=(DeleteRequest&& other) {
  object_path = other.object_path;
  sms_path = other.sms_path;
  callback = std::move(other.callback);
  return *this;
}

FakeModemMessagingClient::DeleteRequest::~DeleteRequest() = default;

}  // namespace ash
