// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_tether_controller.h"

namespace chromeos {
namespace phonehub {

FakeTetherController::FakeTetherController() = default;

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
}  // namespace chromeos
