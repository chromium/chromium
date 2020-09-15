// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_connection_manager.h"

namespace chromeos {
namespace phonehub {

FakeConnectionManager::FakeConnectionManager()
    : status_(Status::kDisconnected) {}

FakeConnectionManager::~FakeConnectionManager() = default;

void FakeConnectionManager::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;
  NotifyStatusChanged();
}

ConnectionManager::Status FakeConnectionManager::GetStatus() const {
  return status_;
}

void FakeConnectionManager::AttemptConnection() {
  ++num_attempt_connection_calls_;
  if (status_ == Status::kDisconnected)
    SetStatus(Status::kConnecting);
}

void FakeConnectionManager::SendMessage(const std::string& payload) {
  sent_messages_.push_back(payload);
}

}  // namespace phonehub
}  // namespace chromeos
