// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_BLUETOOTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_BLUETOOTH_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::settings {

// Chrome OS Bluetooth subpage UI handler.
class BluetoothHandler : public ::settings::SettingsPageUIHandler {
 public:
  BluetoothHandler();
  BluetoothHandler(const BluetoothHandler&) = delete;
  BluetoothHandler& operator=(const BluetoothHandler&) = delete;
  ~BluetoothHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class BluetoothHandlerTest;

  void BluetoothDeviceAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  void HandleRequestFastPairDeviceSupport(const base::Value::List& args);

  void HandleShowBluetoothRevampHatsSurvey(const base::Value::List& args);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::WeakPtrFactory<BluetoothHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_BLUETOOTH_HANDLER_H_
