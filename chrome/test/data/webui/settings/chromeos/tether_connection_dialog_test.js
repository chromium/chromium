// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('TetherConnectionDialog', function() {
  /** @type {!TetherConnectionDialogElement|undefined} */
  let tetherDialog;

  setup(function() {
    tetherDialog = document.createElement('tether-connection-dialog');
    document.body.appendChild(tetherDialog);
    flush();
  });

  test('Battery percentage', function() {
    tetherDialog.managedProperties = {
      name: {
        activeValue: 'name',
      },
      typeProperties: {
        tether: {
          batteryPercentage: 75,
          signalStrength: 0,
        },
      },
    };
    flush();

    const batteryEl = tetherDialog.$.hostDeviceTextBattery;
    assertEquals('75% Battery', batteryEl.innerText.trim());
  });
});
