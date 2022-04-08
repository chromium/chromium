// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';

suite('CellularRoamingToggleButton', function() {
  /** @type {CellularRoamingToggleButton|undefined} */
  let cellularRoamingToggleButton;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {Object} */
  const prefs_ = {
    cros: {
      signed: {
        data_roaming_enabled: {
          value: true,
          controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        },
      },
    },
  };

  function createCellularRoamingToggleButton() {
    cellularRoamingToggleButton =
        document.createElement('cellular-roaming-toggle-button');
    setManagedProperties(
        /* allowRoaming= */ {}, /* roamingState= */ null);
    cellularRoamingToggleButton.prefs = Object.assign({}, prefs_);
    document.body.appendChild(cellularRoamingToggleButton);
    flush();
  }

  /**
   * @param {boolean} allowRoaming
   * @param {string} roamingState
   */
  function setManagedProperties(allowRoaming, roamingState) {
    cellularRoamingToggleButton.managedProperties = {
      type: chromeos.networkConfig.mojom.NetworkType.kCellular,
      typeProperties: {
        cellular: {
          allowRoaming: allowRoaming,
          roamingState: roamingState,
        },
      },
    };
    flush();
  }

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    prefs_.cros.signed.data_roaming_enabled.value = true;
  });

  test('Cellular roaming subtext', function() {
    createCellularRoamingToggleButton();

    // Regardless of whether roaming is enabled or not, the subtext should
    // notify the user if roaming is required by the provider.
    for (const allowRoaming in [true, false]) {
      setManagedProperties(
          /* allowRoaming= */ {activeValue: allowRoaming},
          /* roamingState= */ 'Required');

      assertEquals(
          cellularRoamingToggleButton.i18n('networkAllowDataRoamingRequired'),
          cellularRoamingToggleButton.getSubLabelForTesting());
    }

    // Regardless of the roaming state, except when roaming is required, the
    // subtext should notify the user that roaming is disabled when applicable.
    for (const roamingState in ['Home', 'Roaming']) {
      setManagedProperties(
          /* allowRoaming= */ {activeValue: false},
          /* roamingState= */ roamingState);

      assertEquals(
          cellularRoamingToggleButton.i18n('networkAllowDataRoamingDisabled'),
          cellularRoamingToggleButton.getSubLabelForTesting());
    }

    // Roaming is allowed but we are not roaming.
    setManagedProperties(
        /* allowRoaming= */ {activeValue: true},
        /* roamingState= */ 'Home');

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingEnabledHome'),
        cellularRoamingToggleButton.getSubLabelForTesting());

    // Roaming is allowed and we are roaming.
    setManagedProperties(
        /* allowRoaming= */ {activeValue: true},
        /* roamingState= */ 'Roaming');

    assertEquals(
        cellularRoamingToggleButton.i18n(
            'networkAllowDataRoamingEnabledRoaming'),
        cellularRoamingToggleButton.getSubLabelForTesting());

    // Simulate disabling roaming via policy.
    prefs_.cros.signed.data_roaming_enabled.value = false;
    cellularRoamingToggleButton.prefs = Object.assign({}, prefs_);

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingDisabled'),
        cellularRoamingToggleButton.getSubLabelForTesting());
  });

  suite('Cellular per-network roaming', function() {
    setup(function() {
      mojoApi_.resetForTest();
      createCellularRoamingToggleButton();
    });

    test('Toggle controls property', async function() {
      const cellularRoamingToggle =
          cellularRoamingToggleButton.getCellularRoamingToggle();

      assertFalse(cellularRoamingToggle.checked);
      assertFalse(cellularRoamingToggleButton.isRoamingAllowedForNetwork_);

      cellularRoamingToggle.click();

      await mojoApi_.whenCalled('setProperties');

      assertTrue(cellularRoamingToggle.checked);
      assertTrue(cellularRoamingToggleButton.isRoamingAllowedForNetwork_);
    });

    test('Property reflects managed properties', function() {
      const cellularRoamingToggle =
          cellularRoamingToggleButton.getCellularRoamingToggle();

      assertFalse(cellularRoamingToggle.checked);
      assertFalse(cellularRoamingToggleButton.isRoamingAllowedForNetwork_);

      setManagedProperties(
          /* allowRoaming= */ {activeValue: true},
          /* roamingState= */ 'Home');

      assertTrue(cellularRoamingToggle.checked);
      assertTrue(cellularRoamingToggleButton.isRoamingAllowedForNetwork_);
    });

    test('Roaming disabled when prohibited by policy', function() {
      const cellularRoamingToggle =
          cellularRoamingToggleButton.getCellularRoamingToggle();

      setManagedProperties(
          /* allowRoaming= */ {activeValue: true},
          /* roamingState= */ 'Home');

      assertFalse(cellularRoamingToggleButton.isRoamingProhibitedByPolicy_());

      const dataRoamingEnabled =
          cellularRoamingToggleButton.prefs.cros.signed.data_roaming_enabled;

      dataRoamingEnabled.value = false;

      assertTrue(cellularRoamingToggleButton.isRoamingProhibitedByPolicy_());

      dataRoamingEnabled.controlledBy =
          chrome.settingsPrivate.ControlledBy.USER_POLICY;

      assertFalse(cellularRoamingToggleButton.isRoamingProhibitedByPolicy_());
    });
  });
});
