// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('OsPairedBluetoothListTest', function() {
  /** @type {!SettingsPairedBluetoothListElement|undefined} */
  let pairedBluetoothList;

  setup(function() {
    pairedBluetoothList =
        document.createElement('os-settings-paired-bluetooth-list');
    document.body.appendChild(pairedBluetoothList);
    Polymer.dom.flush();
  });

  test('Base Test', function() {
    const list = pairedBluetoothList.shadowRoot.querySelector('iron-list');
    assertTrue(!!list);
  });
});