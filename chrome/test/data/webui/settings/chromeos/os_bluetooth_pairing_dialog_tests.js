// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';
// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {eventToPromise} from 'chrome://test/test_util.m.js';
// clang-format on

suite('OsBluetoothPairingDialogTest', function() {
  /** @type {?SettingsBluetoothPairingDialogElement} */
  let bluetoothPairingDialog;

  setup(function() {
    bluetoothPairingDialog =
        document.createElement('os-settings-bluetooth-pairing-dialog');
    document.body.appendChild(bluetoothPairingDialog);
    Polymer.dom.flush();
  });

  test('Cancel event is fired on close', async function() {
    const pairingDialog =
        bluetoothPairingDialog.shadowRoot.querySelector('bluetooth-pairing-ui');
    assertTrue(!!pairingDialog);

    const closeBluetoothPairingUiPromise =
        test_util.eventToPromise('close', bluetoothPairingDialog);

    pairingDialog.dispatchEvent(new CustomEvent('cancel'));

    await closeBluetoothPairingUiPromise;
  });
});
