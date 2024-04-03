// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_host_fetcher.h"

#include <memory>

#include "base/functional/callback.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::tether {

TetherHostFetcher::TetherHostFetcher() = default;

TetherHostFetcher::~TetherHostFetcher() = default;

void TetherHostFetcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TetherHostFetcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<multidevice::RemoteDeviceRef> TetherHostFetcher::GetTetherHost() {
  return tether_host_;
}

void TetherHostFetcher::NotifyTetherHostUpdated() {
  for (auto& observer : observers_) {
    observer.OnTetherHostUpdated();
  }
}

}  // namespace ash::tether
