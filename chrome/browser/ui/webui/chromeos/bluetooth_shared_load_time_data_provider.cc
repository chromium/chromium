// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/bluetooth_shared_load_time_data_provider.h"

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace bluetooth {

// Adds the strings needed for bluetooth elements to |html_source|. String ids
// correspond to ids in ui/webui/resources/cr_components/chromeos/bluetooth/.
void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bluetoothPairNewDevice", IDS_BLUETOOTH_PAIRING_PAIR_NEW_DEVICES},
      {"bluetoothAvailableDevices",
       IDS_BLUETOOTH_PAIRING_PAIRING_AVAILABLE_DEVICES},
      {"bluetoothNoAvailableDevices",
       IDS_BLUETOOTH_PAIRING_PAIRING_NO_AVAILABLE_DEVICES},
      {"bluetoothDisabled", IDS_BLUETOOTH_PAIRING_PAIRING_BLUETOOTH_DISABLED},
      {"bluetoothAccept", IDS_BLUETOOTH_PAIRING_ACCEPT_PASSKEY},
      {"bluetoothEnterKey", IDS_BLUETOOTH_PAIRING_ENTER_KEY},
      {"bluetoothPair", IDS_BLUETOOTH_PAIRING_PAIR},
      {"bluetoothReject", IDS_BLUETOOTH_PAIRING_REJECT_PASSKEY},
      {"bluetoothStartConnecting", IDS_BLUETOOTH_PAIRING_START_CONNECTING},
      {"bluetoothEnterPin", IDS_BLUETOOTH_PAIRING_ENTER_PIN},
      {"bluetoothPairedDeviceItemBatteryPercentage",
       IDS_BLUETOOTH_DEVICE_ITEM_BATTERY_PERCENTAGE},
      {"bluetoothPairedDeviceItemLeftBudTrueWirelessBatteryPercentage",
       IDS_BLUETOOTH_DEVICE_ITEM_LEFT_BUD_TRUE_WIRELESS_BATTERY_PERCENTAGE},
      {"bluetoothPairedDeviceItemCaseTrueWirelessBatteryPercentage",
       IDS_BLUETOOTH_DEVICE_ITEM_CASE_TRUE_WIRELESS_BATTERY_PERCENTAGE},
      {"bluetoothPairedDeviceItemRightBudTrueWirelessBatteryPercentage",
       IDS_BLUETOOTH_DEVICE_ITEM_RIGHT_BUD_TRUE_WIRELESS_BATTERY_PERCENTAGE},
      {"bluetoothPairing", IDS_BLUETOOTH_PAIRING_PAIRING},
      {"bluetoothPairingFailed", IDS_BLUETOOTH_PAIRING_PAIRING_FAILED},
      {"bluetoothConfirmCodeMessage",
       IDS_BLUETOOTH_PAIRING_CONFIRM_CODE_MESSAGE},
      {"bluetoothPairingEnterKeys", IDS_BLUETOOTH_PAIRING_ENTER_KEYS},
      {"bluetoothA11yDeviceTypeUnknown",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_UNKNOWN},
      {"bluetoothA11yDeviceTypeComputer",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_COMPUTER},
      {"bluetoothA11yDeviceTypePhone", IDS_BLUETOOTH_A11Y_DEVICE_TYPE_PHONE},
      {"bluetoothA11yDeviceTypeHeadset",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_HEADSET},
      {"bluetoothA11yDeviceTypeVideoCamera",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_VIDEO_CAMERA},
      {"bluetoothA11yDeviceTypeGameController",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_GAME_CONTROLLER},
      {"bluetoothA11yDeviceTypeKeyboard",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD},
      {"bluetoothA11yDeviceTypeKeyboardMouseCombo",
       IDS_BLUETOOTH_A11Y_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO},
      {"bluetoothA11yDeviceTypeMouse", IDS_BLUETOOTH_A11Y_DEVICE_TYPE_MOUSE},
      {"bluetoothA11yDeviceTypeTablet", IDS_BLUETOOTH_A11Y_DEVICE_TYPE_TABLET},
      {"bluetoothA11yDeviceName", IDS_BLUETOOTH_A11Y_DEVICE_NAME},
      {"bluetoothPairingDescription", IDS_BLUETOOTH_PAIRING_DESCRIPTION},
      {"bluetoothPairingDeviceItemSecondaryErrorA11YLabel",
       IDS_BLUETOOTH_PAIRINGS_DEVICE_ITEM_SECONDARY_ERROR_A11Y_LABEL},
      {"bluetoothPairingDeviceItemSecondaryPairingA11YLabel",
       IDS_BLUETOOTH_PAIRINGS_DEVICE_ITEM_SECONDARY_PAIRING_A11Y_LABEL},
      // Device connecting and pairing.
      // These ids are generated in JS using 'bluetooth_' + a value from
      // bluetoothPrivate.PairingEventType (see bluetooth_private.idl).
      // 'requestAuthorization' has no associated message.
      {"bluetooth_requestPincode", IDS_BLUETOOTH_PAIRING_REQUEST_PINCODE},
      {"bluetooth_displayPincode", IDS_BLUETOOTH_PAIRING_DISPLAY_PINCODE},
      {"bluetooth_requestPasskey", IDS_BLUETOOTH_PAIRING_REQUEST_PASSKEY},
      {"bluetooth_displayPasskey", IDS_BLUETOOTH_PAIRING_DISPLAY_PASSKEY},
      {"bluetooth_confirmPasskey", IDS_BLUETOOTH_PAIRING_CONFIRM_PASSKEY},
      // Also display the IDS_BLUETOOTH_PAIRING_DISPLAY_PASSKEY for the
      // 'keysEntered' event: continue prompting the user to enter the passkey
      // as they continue to enter its keys.
      {"bluetooth_keysEntered", IDS_BLUETOOTH_PAIRING_DISPLAY_PASSKEY},
      // These ids are generated in JS using 'bluetooth_connect_' + a value from
      // bluetoothPrivate.ConnectResultType (see bluetooth_private.idl).
      {"bluetooth_connect_attributeLengthInvalid",
       IDS_BLUETOOTH_PAIRING_CONNECT_ATTRIBUTE_LENGTH_INVALID},
      {"bluetooth_connect_authCanceled",
       IDS_BLUETOOTH_PAIRING_CONNECT_AUTH_CANCELED},
      {"bluetooth_connect_authFailed",
       IDS_BLUETOOTH_PAIRING_CONNECT_AUTH_FAILED},
      {"bluetooth_connect_authRejected",
       IDS_BLUETOOTH_PAIRING_CONNECT_AUTH_REJECTED},
      {"bluetooth_connect_authTimeout",
       IDS_BLUETOOTH_PAIRING_CONNECT_AUTH_TIMEOUT},
      {"bluetooth_connect_connectionCongested",
       IDS_BLUETOOTH_PAIRING_CONNECT_CONNECTION_CONGESTED},
      {"bluetooth_connect_failed", IDS_BLUETOOTH_PAIRING_CONNECT_FAILED},
      {"bluetooth_connect_inProgress",
       IDS_BLUETOOTH_PAIRING_CONNECT_IN_PROGRESS},
      {"bluetooth_connect_insufficientEncryption",
       IDS_BLUETOOTH_PAIRING_CONNECT_INSUFFICIENT_ENCRYPTION},
      {"bluetooth_connect_offsetInvalid",
       IDS_BLUETOOTH_PAIRING_CONNECT_OFFSET_INVALID},
      {"bluetooth_connect_readNotPermitted",
       IDS_BLUETOOTH_PAIRING_CONNECT_READ_NOT_PERMITTED},
      {"bluetooth_connect_requestNotSupported",
       IDS_BLUETOOTH_PAIRING_CONNECT_REQUEST_NOT_SUPPORTED},
      {"bluetooth_connect_unsupportedDevice",
       IDS_BLUETOOTH_PAIRING_CONNECT_UNSUPPORTED_DEVICE},
      {"bluetooth_connect_writeNotPermitted",
       IDS_BLUETOOTH_PAIRING_CONNECT_WRITE_NOT_PERMITTED},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddString(
      "bluetoothPairingLearnMoreLabel",
      l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_PAIRING_LEARN_MORE,
          base::ASCIIToUTF16(chrome::kBluetoothPairingLearnMoreUrl)));
}

void AddLoadTimeData(content::WebUIDataSource* html_source) {
  AddLocalizedStrings(html_source);

  html_source->AddBoolean("enableBluetoothRevamp",
                          chromeos::features::IsBluetoothRevampEnabled());
}

}  // namespace bluetooth
}  // namespace chromeos
