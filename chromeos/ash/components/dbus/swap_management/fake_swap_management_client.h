// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_FAKE_SWAP_MANAGEMENT_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_FAKE_SWAP_MANAGEMENT_CLIENT_H_

#include "chromeos/ash/components/dbus/swap_management/swap_management_client.h"

namespace ash {

// The SwapManagementClient implementation used on Linux desktop,
// which does nothing.
class COMPONENT_EXPORT(SWAP_MANAGEMENT) FakeSwapManagementClient
    : public SwapManagementClient {
 public:
  FakeSwapManagementClient();

  FakeSwapManagementClient(const FakeSwapManagementClient&) = delete;
  FakeSwapManagementClient& operator=(const FakeSwapManagementClient&) = delete;

  ~FakeSwapManagementClient() override;

  void Init(dbus::Bus* bus) override;

  void MGLRUSetEnable(uint8_t value,
                      chromeos::VoidDBusMethodCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SWAP_MANAGEMENT_FAKE_SWAP_MANAGEMENT_CLIENT_H_
