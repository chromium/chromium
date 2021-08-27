// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothBasePageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_base_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsBluetoothBasePageTest', function() {
  /** @type {?SettingsBluetoothBasePageElement} */
  let bluetoothBasePage;

  setup(function() {
    bluetoothBasePage = /** @type {?SettingsBluetoothBasePageElement} */ (
        document.createElement('bluetooth-base-page'));
    document.body.appendChild(bluetoothBasePage);
    flush();
  });

  test('Title is shown', function() {
    const title = bluetoothBasePage.shadowRoot.querySelector('#title');
    assertTrue(!!title);
    assertEquals(
        bluetoothBasePage.i18n('bluetoothPairNewDevice'),
        title.textContent.trim());
  });
});