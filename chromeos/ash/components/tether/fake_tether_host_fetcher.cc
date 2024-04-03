// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"

#include "base/memory/ptr_util.h"

namespace ash::tether {

FakeTetherHostFetcher::FakeTetherHostFetcher(
    std::optional<multidevice::RemoteDeviceRef> tether_host) {
  tether_host_ = tether_host;
}

FakeTetherHostFetcher::~FakeTetherHostFetcher() = default;

void FakeTetherHostFetcher::SetTetherHost(
    const std::optional<multidevice::RemoteDeviceRef> tether_host) {
  tether_host_ = tether_host;
  NotifyTetherHostUpdated();
}

}  // namespace ash::tether
