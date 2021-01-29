// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_profile_handler.h"

#include "chromeos/network/cellular_esim_profile.h"

namespace chromeos {

CellularESimProfileHandler::CellularESimProfileHandler() = default;

CellularESimProfileHandler::~CellularESimProfileHandler() = default;

void CellularESimProfileHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CellularESimProfileHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void CellularESimProfileHandler::NotifyESimProfileListUpdated() {
  for (auto& observer : observer_list_)
    observer.OnESimProfileListUpdated();
}

}  // namespace chromeos
