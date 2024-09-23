// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_

#include <vector>

#include "chromeos/ash/components/tether/tether_host_fetcher.h"

namespace ash::tether {

// Test double for TetherHostFetcher.
class FakeTetherHostFetcher : public TetherHostFetcher {
 public:
  explicit FakeTetherHostFetcher(
      std::optional<multidevice::RemoteDeviceRef> tether_host);

  FakeTetherHostFetcher(const FakeTetherHostFetcher&) = delete;
  FakeTetherHostFetcher& operator=(const FakeTetherHostFetcher&) = delete;

  ~FakeTetherHostFetcher() override;

  void SetTetherHost(
      const std::optional<multidevice::RemoteDeviceRef> tether_host);
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_HOST_FETCHER_H_
