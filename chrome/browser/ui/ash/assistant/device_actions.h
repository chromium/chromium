// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_

#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

class DeviceActions : public chromeos::assistant::mojom::DeviceActions {
 public:
  DeviceActions();
  ~DeviceActions() override;

  // mojom::DeviceActions overrides:
  void SetWifiEnabled(bool enabled) override;
  void SetBluetoothEnabled(bool enabled) override;
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override;
  void SetScreenBrightnessLevel(double level, bool gradual) override;
  void SetNightLightEnabled(bool enabled) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceActions);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_
