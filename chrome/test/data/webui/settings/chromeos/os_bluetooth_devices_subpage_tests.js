// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('OsBluetoothDevicesSubpageTest', function() {
  /** @type {!SettingsBluetoothDevicesSubpageElement|undefined} */
  let bluetoothDevicesSubpage;

  setup(function() {
    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    document.body.appendChild(bluetoothDevicesSubpage);
    Polymer.dom.flush();
  });

  test('Base Test', function() {
    assertTrue(!!bluetoothDevicesSubpage);
  });
});