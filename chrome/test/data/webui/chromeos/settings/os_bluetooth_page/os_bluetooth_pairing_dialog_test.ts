// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsBluetoothPairingDialogElement} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/chromeos/bluetooth/fake_bluetooth_config.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<os-settings-bluetooth-pairing-dialog>', () => {
  let bluetoothPairingDialog: SettingsBluetoothPairingDialogElement;
  let bluetoothConfig: FakeBluetoothConfig;

  setup(() => {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);

    bluetoothPairingDialog =
        document.createElement('os-settings-bluetooth-pairing-dialog');
    document.body.appendChild(bluetoothPairingDialog);
    flush();
  });

  teardown(() => {
    bluetoothPairingDialog.remove();
  });

  test('Finished event is fired on close', async () => {
    const pairingDialog = bluetoothPairingDialog.shadowRoot!.querySelector(
        'bluetooth-pairing-ui');
    assertTrue(!!pairingDialog);

    const closeBluetoothPairingUiPromise =
        eventToPromise('close', bluetoothPairingDialog);

    pairingDialog.dispatchEvent(new CustomEvent('finished'));

    await closeBluetoothPairingUiPromise;
  });
});
