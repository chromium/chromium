// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"

#include "base/memory/ptr_util.h"

namespace ash {

namespace tether {

FakeTetherHostFetcher::FakeTetherHostFetcher(
    const multidevice::RemoteDeviceRefList& tether_hosts)
    : tether_hosts_(tether_hosts) {}

FakeTetherHostFetcher::FakeTetherHostFetcher()
    : FakeTetherHostFetcher(multidevice::RemoteDeviceRefList()) {}

FakeTetherHostFetcher::~FakeTetherHostFetcher() = default;

void FakeTetherHostFetcher::NotifyTetherHostsUpdated() {
  TetherHostFetcher::NotifyTetherHostsUpdated();
}

bool FakeTetherHostFetcher::HasSyncedTetherHosts() {
  return !tether_hosts_.empty();
}

void FakeTetherHostFetcher::FetchAllTetherHosts(
    TetherHostFetcher::TetherHostListCallback callback) {
  ProcessFetchAllTetherHostsRequest(tether_hosts_, std::move(callback));
}

void FakeTetherHostFetcher::FetchTetherHost(
    const std::string& device_id,
    TetherHostFetcher::TetherHostCallback callback) {
  ProcessFetchSingleTetherHostRequest(device_id, tether_hosts_,
                                      std::move(callback));
}

}  // namespace tether

}  // namespace ash
