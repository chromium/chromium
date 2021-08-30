// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingDeviceSelectionPageUiElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_device_selection_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsBluetoothPairingDeviceSelectionPageTest', function() {
  /** @type {?SettingsBluetoothPairingDeviceSelectionPageUiElement} */
  let deviceSelectionPage;

  setup(function() {
    deviceSelectionPage =
        /** @type {?SettingsBluetoothPairingDeviceSelectionPageUiElement} */ (
            document.createElement('bluetooth-pairing-device-selection-page'));
    document.body.appendChild(deviceSelectionPage);
    flush();
  });

  test('Base test', function() {
    const basePage =
        deviceSelectionPage.shadowRoot.querySelector('bluetooth-base-page');
    assertTrue(!!basePage);
  });
});
