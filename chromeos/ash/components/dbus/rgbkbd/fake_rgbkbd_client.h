// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"

namespace ash {

class COMPONENT_EXPORT(RGBKBD) FakeRgbkbdClient : public RgbkbdClient {
 public:
  FakeRgbkbdClient();
  FakeRgbkbdClient(const FakeRgbkbdClient&) = delete;
  FakeRgbkbdClient& operator=(const FakeRgbkbdClient&) = delete;
  ~FakeRgbkbdClient() override;

  void GetRgbKeyboardCapabilities(
      GetRgbKeyboardCapabilitiesCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
