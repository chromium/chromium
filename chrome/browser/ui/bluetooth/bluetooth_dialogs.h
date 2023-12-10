// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_DIALOGS_H_
#define CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_DIALOGS_H_

#include <optional>
#include <string>

#include "content/public/browser/bluetooth_delegate.h"

namespace content {
class WebContents;
}  // namespace content

#if PAIR_BLUETOOTH_ON_DEMAND()
// Shows the dialog to request the Bluetooth credentials for the device
// identified by |device_identifier|. |device_identifier| is the most
// appropriate string to display to the user for device identification
// (e.g. name, MAC address).
void ShowBluetoothDeviceCredentialsDialog(
    content::WebContents* web_contents,
    const std::u16string& device_identifier,
    content::BluetoothDelegate::PairPromptCallback close_callback);

// Show a user prompt for pairing a Bluetooth device. |device_identifier|
// is the most appropriate string to display for device identification
// (e.g. name, MAC address). The |pin| is displayed (if specified),
// so the user can confirm a matching value is displayed on the device.
void ShowBluetoothDevicePairConfirmDialog(
    content::WebContents* web_contents,
    const std::u16string& device_identifier,
    const std::optional<std::u16string>& pin,
    content::BluetoothDelegate::PairPromptCallback close_callback);
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

#endif  // CHROME_BROWSER_UI_BLUETOOTH_BLUETOOTH_DIALOGS_H_
