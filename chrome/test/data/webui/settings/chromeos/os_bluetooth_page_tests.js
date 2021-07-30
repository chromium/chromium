// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// clang-format on

suite('OsBluetoothPageTest', function() {
  /** @type {!SettingsBluetoothPageElement|undefined} */
  let bluetoothPage;

  setup(function() {
    // TODO(crbug.com/1010321): Replace this with fake_cros_bluetooth_config
    // when it is created.
    setBluetoothConfigForTesting({observeSystemProperties: (observer) => {}});
    bluetoothPage = document.createElement('os-settings-bluetooth-page');
    document.body.appendChild(bluetoothPage);
    Polymer.dom.flush();
  });

  test('Base Test', function() {
    const bluetoothSummary = bluetoothPage.$$('os-settings-bluetooth-summary');
    assertTrue(!!bluetoothSummary);
  });
});