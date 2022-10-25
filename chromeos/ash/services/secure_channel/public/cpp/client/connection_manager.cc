// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash::secure_channel {

ConnectionManager::ConnectionManager() = default;
ConnectionManager::~ConnectionManager() = default;

void ConnectionManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConnectionManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConnectionManager::NotifyStatusChanged() {
  for (auto& observer : observer_list_)
    observer.OnConnectionStatusChanged();
}

void ConnectionManager::NotifyMessageReceived(const std::string& payload) {
  for (auto& observer : observer_list_)
    observer.OnMessageReceived(payload);
}

std::ostream& operator<<(std::ostream& stream,
                         ConnectionManager::Status status) {
  switch (status) {
    case ConnectionManager::Status::kConnecting:
      stream << "[Connecting]";
      break;
    case ConnectionManager::Status::kConnected:
      stream << "[Connected]";
      break;
    case ConnectionManager::Status::kDisconnected:
      stream << "[Disconnected]";
      break;
  }
  return stream;
}

}  // namespace ash::secure_channel
