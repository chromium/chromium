// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_CHROME_BLUETOOTH_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_BLUETOOTH_CHROME_BLUETOOTH_CHOOSER_CONTROLLER_H_

#include "components/permissions/bluetooth_chooser_controller.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// The concrete version of BluetoothChooserController for Chrome.
class ChromeBluetoothChooserController
    : public permissions::BluetoothChooserController {
 public:
  ChromeBluetoothChooserController(
      content::RenderFrameHost* owner,
      const content::BluetoothChooser::EventHandler& event_handler);

  ChromeBluetoothChooserController(const ChromeBluetoothChooserController&) =
      delete;
  ChromeBluetoothChooserController& operator=(
      const ChromeBluetoothChooserController&) = delete;

  ~ChromeBluetoothChooserController() override;

  // Provides help information when the adapter is off.
  void OpenAdapterOffHelpUrl() const override;

  // Navigate user to preferences in order to acquire Bluetooth permission.
  void OpenPermissionPreferences() const override;

  // Opens the help center URL.
  void OpenHelpCenterUrl() const override;
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_CHROME_BLUETOOTH_CHOOSER_CONTROLLER_H_
