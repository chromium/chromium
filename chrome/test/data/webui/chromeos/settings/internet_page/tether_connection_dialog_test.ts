// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {TetherConnectionDialogElement} from 'chrome://os-settings/lazy_load.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('<tether-connection-dialog>', function() {
  let tetherDialog: TetherConnectionDialogElement;

  setup(function() {
    tetherDialog = document.createElement('tether-connection-dialog');
    document.body.appendChild(tetherDialog);
    flush();
  });

  test('Battery percentage', function() {
    const managedProperties = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString('Tether'), 'guid', 'name');
    managedProperties.typeProperties.tether!.batteryPercentage = 75;
    tetherDialog.managedProperties = managedProperties;
    flush();

    const batteryEl = tetherDialog.shadowRoot!.querySelector<HTMLElement>(
        '#hostDeviceTextBattery');
    assertEquals('75% Battery', batteryEl!.innerText.trim());
  });
});
