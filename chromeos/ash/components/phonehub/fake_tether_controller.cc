// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_tether_controller.h"

namespace ash {
namespace phonehub {

FakeTetherController::FakeTetherController()
    : status_(Status::kConnectionAvailable) {}

FakeTetherController::~FakeTetherController() = default;

void FakeTetherController::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;
  NotifyStatusChanged();
}

TetherController::Status FakeTetherController::GetStatus() const {
  return status_;
}

void FakeTetherController::ScanForAvailableConnection() {
  num_scan_for_available_connection_calls_++;
  if (status_ == Status::kConnectionUnavailable)
    SetStatus(Status::kConnectionAvailable);
}

void FakeTetherController::AttemptConnection() {
  if (status_ == Status::kConnectionUnavailable ||
      status_ == Status::kConnectionAvailable) {
    SetStatus(Status::kConnecting);
  }
}

void FakeTetherController::Disconnect() {
  if (status_ == Status::kConnecting || status_ == Status::kConnected)
    SetStatus(Status::kConnecting);
}

}  // namespace phonehub
}  // namespace ash
