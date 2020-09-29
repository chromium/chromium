// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('TetherConnectionDialog', function() {
  /** @type {!TetherConnectionDialogElement|undefined} */
  let tetherDialog;

  setup(function() {
    tetherDialog = document.createElement('tether-connection-dialog');
    document.body.appendChild(tetherDialog);
    Polymer.dom.flush();
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
    Polymer.dom.flush();

    const batteryEl = tetherDialog.$.hostDeviceTextBattery;
    assertEquals('75% Battery', batteryEl.innerText.trim());
  });
});
