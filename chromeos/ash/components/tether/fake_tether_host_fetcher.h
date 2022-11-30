// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_

#include <vector>

#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/tether_host_fetcher.h"

namespace ash {

namespace tether {

// Test double for TetherHostFetcher.
class FakeTetherHostFetcher : public TetherHostFetcher {
 public:
  explicit FakeTetherHostFetcher(
      const multidevice::RemoteDeviceRefList& tether_hosts);
  FakeTetherHostFetcher();

  FakeTetherHostFetcher(const FakeTetherHostFetcher&) = delete;
  FakeTetherHostFetcher& operator=(const FakeTetherHostFetcher&) = delete;

  ~FakeTetherHostFetcher() override;

  void set_tether_hosts(const multidevice::RemoteDeviceRefList& tether_hosts) {
    tether_hosts_ = tether_hosts;
  }

  void NotifyTetherHostsUpdated();

  // TetherHostFetcher:
  bool HasSyncedTetherHosts() override;
  void FetchAllTetherHosts(
      TetherHostFetcher::TetherHostListCallback callback) override;
  void FetchTetherHost(const std::string& device_id,
                       TetherHostFetcher::TetherHostCallback callback) override;

 private:
  multidevice::RemoteDeviceRefList tether_hosts_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_
