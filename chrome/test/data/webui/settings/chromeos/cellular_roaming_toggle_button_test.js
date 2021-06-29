// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CellularRoamingToggleButton', function() {
  /** @type {CellularRoamingToggleButton|undefined} */
  let cellularRoamingToggleButton;

  setup(function() {
    cellularRoamingToggleButton =
        document.createElement('cellular-roaming-toggle-button');
    document.body.appendChild(cellularRoamingToggleButton);
    Polymer.dom.flush();
  });

  /**
   * @param {boolean} allowRoaming
   * @param {string} roamingState
   */
  function setManagedProperties(allowRoaming, roamingState) {
    cellularRoamingToggleButton.managedProperties = {
      typeProperties: {
        cellular: {
          allowRoaming: allowRoaming,
          roamingState: roamingState,
        },
      },
    };
    Polymer.dom.flush();
  }

  test('Cellular roaming subtext', function() {
    const cellularRoamingToggle =
        cellularRoamingToggleButton.getCellularRoamingToggle();

    setManagedProperties(
        /* allowRoaming= */ false, /* roamingState= */ null);

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingDisabled'),
        cellularRoamingToggle.subLabel);

    setManagedProperties(
        /* allowRoaming= */ true, /* roamingState= */ 'Home');

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingEnabledHome'),
        cellularRoamingToggle.subLabel);

    setManagedProperties(
        /* allowRoaming= */ true, /* roamingState= */ 'Roaming');

    assertEquals(
        cellularRoamingToggleButton.i18n(
            'networkAllowDataRoamingEnabledRoaming'),
        cellularRoamingToggle.subLabel);
  });
});
