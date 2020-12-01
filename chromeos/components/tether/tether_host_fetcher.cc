// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_host_fetcher.h"

#include <memory>

#include "base/callback.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace tether {

TetherHostFetcher::TetherHostFetcher() = default;

TetherHostFetcher::~TetherHostFetcher() = default;

void TetherHostFetcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TetherHostFetcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TetherHostFetcher::ProcessFetchAllTetherHostsRequest(
    const multidevice::RemoteDeviceRefList& remote_device_list,
    TetherHostListCallback callback) {
  std::move(callback).Run(remote_device_list);
}

void TetherHostFetcher::ProcessFetchSingleTetherHostRequest(
    const std::string& device_id,
    const multidevice::RemoteDeviceRefList& remote_device_list,
    TetherHostCallback callback) {
  for (auto remote_device : remote_device_list) {
    if (remote_device.GetDeviceId() == device_id) {
      std::move(callback).Run(
          base::make_optional<multidevice::RemoteDeviceRef>(remote_device));
      return;
    }
  }

  std::move(callback).Run(base::nullopt);
}

void TetherHostFetcher::NotifyTetherHostsUpdated() {
  for (auto& observer : observers_)
    observer.OnTetherHostsUpdated();
}

}  // namespace tether

}  // namespace chromeos
