// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_BEAM_FAKE_ZR_VENDOR_OS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_BEAM_FAKE_ZR_VENDOR_OS_CLIENT_H_

#include "chromeos/ash/components/dbus/beam/zr_vendor_os_client.h"

namespace ash {

class COMPONENT_EXPORT(ZRVENDOROS) FakeZrVendorOsClient
    : public ZrVendorOsClient {
 public:
  FakeZrVendorOsClient();
  FakeZrVendorOsClient(const FakeZrVendorOsClient&) = delete;
  FakeZrVendorOsClient& operator=(const FakeZrVendorOsClient&) = delete;
  ~FakeZrVendorOsClient() override;

  // ZrVendorOsClient:
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void Init(dbus::Bus* bus) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_BEAM_FAKE_ZR_VENDOR_OS_CLIENT_H_
