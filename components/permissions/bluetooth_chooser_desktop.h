// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_BLUETOOTH_CHOOSER_DESKTOP_H_
#define COMPONENTS_PERMISSIONS_BLUETOOTH_CHOOSER_DESKTOP_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace permissions {

class BluetoothChooserController;
class ChooserController;

// Represents a Bluetooth chooser to ask the user to select a Bluetooth
// device from a list of options. This implementation is for desktop.
// BluetoothChooserAndroid implements the mobile part.
class BluetoothChooserDesktop : public content::BluetoothChooser {
 public:
  // The OnceClosure returned by |show_dialog_callback| can be invoked to close
  // the dialog. It should be a no-op to invoke the closure if the dialog has
  // already been closed by the user.
  BluetoothChooserDesktop(
      std::unique_ptr<permissions::BluetoothChooserController> controller,
      base::OnceCallback<
          base::OnceClosure(std::unique_ptr<permissions::ChooserController>)>
          show_dialog_callback);

  BluetoothChooserDesktop(const BluetoothChooserDesktop&) = delete;
  BluetoothChooserDesktop& operator=(const BluetoothChooserDesktop&) = delete;

  ~BluetoothChooserDesktop() override;

  // BluetoothChooser:
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

 private:
  // DeviceChooserContentView owns the controller.
  base::WeakPtr<BluetoothChooserController> bluetooth_chooser_controller_;

  // Closes the displayed UI if it is still open. Used to ensure the bubble
  // closes if this controller is torn down.
  base::ScopedClosureRunner close_closure_runner_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_BLUETOOTH_CHOOSER_DESKTOP_H_
