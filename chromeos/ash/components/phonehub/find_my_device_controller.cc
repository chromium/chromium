// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/find_my_device_controller.h"

namespace ash {
namespace phonehub {

FindMyDeviceController::FindMyDeviceController() = default;

FindMyDeviceController::~FindMyDeviceController() = default;

void FindMyDeviceController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FindMyDeviceController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FindMyDeviceController::NotifyPhoneRingingStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnPhoneRingingStateChanged();
}

std::ostream& operator<<(std::ostream& stream,
                         FindMyDeviceController::Status status) {
  switch (status) {
    case FindMyDeviceController::Status::kRingingOff:
      stream << "[Ringing Off]";
      break;
    case FindMyDeviceController::Status::kRingingOn:
      stream << "[Ringing On]";
      break;
    case FindMyDeviceController::Status::kRingingNotAvailable:
      stream << "[Ringing Not Available]";
      break;
  }
  return stream;
}

}  // namespace phonehub
}  // namespace ash
