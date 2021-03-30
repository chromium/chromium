// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/bluetooth_dialog_localized_strings_provider.h"

#include "build/build_config.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace bluetooth_dialog {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bluetoothAccept", IDS_SETTINGS_BLUETOOTH_ACCEPT_PASSKEY},
      {"bluetoothEnterKey", IDS_SETTINGS_BLUETOOTH_ENTER_KEY},
      {"bluetoothPair", IDS_SETTINGS_BLUETOOTH_PAIR},
      {"bluetoothReject", IDS_SETTINGS_BLUETOOTH_REJECT_PASSKEY},
      {"bluetoothStartConnecting", IDS_SETTINGS_BLUETOOTH_START_CONNECTING},
      // Device connecting and pairing.
      // These ids are generated in JS using 'bluetooth_' + a value from
      // bluetoothPrivate.PairingEventType (see bluetooth_private.idl).
      // 'requestAuthorization' has no associated message.
      {"bluetooth_requestPincode", IDS_SETTINGS_BLUETOOTH_REQUEST_PINCODE},
      {"bluetooth_displayPincode", IDS_SETTINGS_BLUETOOTH_DISPLAY_PINCODE},
      {"bluetooth_requestPasskey", IDS_SETTINGS_BLUETOOTH_REQUEST_PASSKEY},
      {"bluetooth_displayPasskey", IDS_SETTINGS_BLUETOOTH_DISPLAY_PASSKEY},
      {"bluetooth_confirmPasskey", IDS_SETTINGS_BLUETOOTH_CONFIRM_PASSKEY},
      // Also display the IDS_SETTINGS_BLUETOOTH_DISPLAY_PASSKEY for the
      // 'keysEntered' event: continue prompting the user to enter the passkey
      // as they continue to enter its keys.
      {"bluetooth_keysEntered", IDS_SETTINGS_BLUETOOTH_DISPLAY_PASSKEY},
      // These ids are generated in JS using 'bluetooth_connect_' + a value from
      // bluetoothPrivate.ConnectResultType (see bluetooth_private.idl).
      {"bluetooth_connect_attributeLengthInvalid",
       IDS_SETTINGS_BLUETOOTH_CONNECT_ATTRIBUTE_LENGTH_INVALID},
      {"bluetooth_connect_authCanceled",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_CANCELED},
      {"bluetooth_connect_authFailed",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_FAILED},
      {"bluetooth_connect_authRejected",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_REJECTED},
      {"bluetooth_connect_authTimeout",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_TIMEOUT},
      {"bluetooth_connect_connectionCongested",
       IDS_SETTINGS_BLUETOOTH_CONNECT_CONNECTION_CONGESTED},
      {"bluetooth_connect_failed", IDS_SETTINGS_BLUETOOTH_CONNECT_FAILED},
      {"bluetooth_connect_inProgress",
       IDS_SETTINGS_BLUETOOTH_CONNECT_IN_PROGRESS},
      {"bluetooth_connect_insufficientEncryption",
       IDS_SETTINGS_BLUETOOTH_CONNECT_INSUFFICIENT_ENCRYPTION},
      {"bluetooth_connect_offsetInvalid",
       IDS_SETTINGS_BLUETOOTH_CONNECT_OFFSET_INVALID},
      {"bluetooth_connect_readNotPermitted",
       IDS_SETTINGS_BLUETOOTH_CONNECT_READ_NOT_PERMITTED},
      {"bluetooth_connect_requestNotSupported",
       IDS_SETTINGS_BLUETOOTH_CONNECT_REQUEST_NOT_SUPPORTED},
      {"bluetooth_connect_unsupportedDevice",
       IDS_SETTINGS_BLUETOOTH_CONNECT_UNSUPPORTED_DEVICE},
      {"bluetooth_connect_writeNotPermitted",
       IDS_SETTINGS_BLUETOOTH_CONNECT_WRITE_NOT_PERMITTED},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace bluetooth_dialog
}  // namespace chromeos
