// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_BLUETOOTH_CHOOSER_CONTROLLER_H_
#define COMPONENTS_PERMISSIONS_BLUETOOTH_CHOOSER_CONTROLLER_H_

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/permissions/chooser_controller.h"
#include "content/public/browser/bluetooth_chooser.h"

namespace content {
class RenderFrameHost;
}

namespace permissions {

// BluetoothChooserController is a chooser that presents a list of
// Bluetooth device names, which come from |bluetooth_chooser_desktop_|.
// It can be used by WebBluetooth API to get the user's permission to
// access a Bluetooth device.
class BluetoothChooserController : public ChooserController {
 public:
  BluetoothChooserController(
      content::RenderFrameHost* owner,
      const content::BluetoothChooser::EventHandler& event_handler,
      std::u16string title);

  BluetoothChooserController(const BluetoothChooserController&) = delete;
  BluetoothChooserController& operator=(const BluetoothChooserController&) =
      delete;

  ~BluetoothChooserController() override;

  // ChooserController:
  bool ShouldShowIconBeforeText() const override;
  bool ShouldShowReScanButton() const override;
  std::u16string GetNoOptionsText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  size_t NumOptions() const override;
  int GetSignalStrengthLevel(size_t index) const override;
  bool IsConnected(size_t index) const override;
  bool IsPaired(size_t index) const override;
  std::u16string GetOption(size_t index) const override;
  void RefreshOptions() override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  bool ShouldShowAdapterOffView() const override;
  int GetAdapterOffMessageId() const override;
  int GetTurnAdapterOnLinkTextMessageId() const override;
  bool ShouldShowAdapterUnauthorizedView() const override;
  int GetBluetoothUnauthorizedMessageId() const override;
  int GetAuthorizeBluetoothLinkTextMessageId() const override;

  // Update the state of the Bluetooth adapter.
  void OnAdapterPresenceChanged(
      content::BluetoothChooser::AdapterPresence presence);

  // Update the Bluetooth discovery state and let the user know whether
  // discovery is happening.
  void OnDiscoveryStateChanged(content::BluetoothChooser::DiscoveryState state);

  // Shows a new device in the chooser or updates its information.
  // The range of |signal_strength_level| is -1 to 4 inclusively.
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level);

  // Tells the chooser that a device is no longer available.
  void RemoveDevice(const std::string& device_id);

  // Called when |event_handler_| is no longer valid and should not be used
  // any more.
  void ResetEventHandler();

  // Get a weak pointer to this controller.
  base::WeakPtr<BluetoothChooserController> GetWeakPtr();

 private:
  struct BluetoothDeviceInfo {
    std::string id;
    int signal_strength_level;
    bool is_connected;
    bool is_paired;
  };

  // Clears |device_names_and_ids_| and |device_name_counts_|. Called when
  // Bluetooth adapter is turned on or off, or when re-scan happens.
  void ClearAllDevices();

  std::vector<BluetoothDeviceInfo> devices_;
  std::unordered_map<std::string, std::u16string> device_id_to_name_map_;
  // Maps from device name to number of devices with that name.
  std::unordered_map<std::u16string, int> device_name_counts_;

  content::BluetoothChooser::EventHandler event_handler_;

  base::WeakPtrFactory<BluetoothChooserController> weak_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_BLUETOOTH_CHOOSER_CONTROLLER_H_
