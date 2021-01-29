// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_CHROME_EXTENSION_BLUETOOTH_CHOOSER_H_
#define CHROME_BROWSER_UI_BLUETOOTH_CHROME_EXTENSION_BLUETOOTH_CHOOSER_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/bluetooth_chooser.h"

class BluetoothChooserController;
class ChromeExtensionChooserDialog;

namespace content {
class RenderFrameHost;
}

// Represents a Bluetooth chooser to ask the user to select a Bluetooth
// device from a list of options. This implementation is for extensions.
class ChromeExtensionBluetoothChooser : public content::BluetoothChooser {
 public:
  ChromeExtensionBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler);
  ~ChromeExtensionBluetoothChooser() override;

  // content::BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

 private:
  // Weak. DeviceChooserContentView[Cocoa] owns it.
  BluetoothChooserController* bluetooth_chooser_controller_;
  std::unique_ptr<ChromeExtensionChooserDialog> chooser_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionBluetoothChooser);
};

#endif  // CHROME_BROWSER_UI_BLUETOOTH_CHROME_EXTENSION_BLUETOOTH_CHOOSER_H_
