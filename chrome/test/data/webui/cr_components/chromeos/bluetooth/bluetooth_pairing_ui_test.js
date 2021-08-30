// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothPairingUiElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_pairing_ui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsBluetoothPairingUiTest', function() {
  /** @type {?SettingsBluetoothPairingUiElement} */
  let bluetoothPairingUi;

  setup(function() {
    bluetoothPairingUi = /** @type {?SettingsBluetoothPairingUiElement} */ (
        document.createElement('bluetooth-pairing-ui'));
    document.body.appendChild(bluetoothPairingUi);
    flush();
  });

  test('Base test', function() {
    const deviceSelectionPage =
        bluetoothPairingUi.shadowRoot.querySelector('#deviceSelectionPage');
    assertTrue(!!deviceSelectionPage);
  });
});
