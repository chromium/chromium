// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/tether_controller.h"

namespace chromeos {
namespace phonehub {

TetherController::TetherController() = default;

TetherController::~TetherController() = default;

void TetherController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TetherController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TetherController::NotifyStatusChanged() {
  for (auto& observer : observer_list_)
    observer.OnTetherStatusChanged();
}

std::ostream& operator<<(std::ostream& stream,
                         TetherController::Status status) {
  switch (status) {
    case TetherController::Status::kIneligibleForFeature:
      stream << "[Ineligible for feature]";
      break;
    case TetherController::Status::kConnectionUnavailable:
      stream << "[Connection unavailable]";
      break;
    case TetherController::Status::kConnectionAvailable:
      stream << "[Connection available]";
      break;
    case TetherController::Status::kConnecting:
      stream << "[Connecting]";
      break;
    case TetherController::Status::kConnected:
      stream << "[Connected]";
      break;
  }
  return stream;
}

}  // namespace phonehub
}  // namespace chromeos
