// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// clang-format on

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
    Polymer.dom.flush();
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
    Polymer.dom.flush();
  }

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  setup(function() {
    prefs_.cros.signed.data_roaming_enabled.value = true;
  });

  // Run this test twice, once with the per-network cellular roaming feature
  // flag enabled and a second time with it disabled.
  [true, false].forEach(
      (allowPerNetworkRoaming) => test(
          `Cellular roaming subtext with per-network roaming ${
              allowPerNetworkRoaming ? 'enabled' : 'disabled'}`,
          function() {
            loadTimeData.overrideValues({
              allowPerNetworkRoaming: allowPerNetworkRoaming,
            });
            createCellularRoamingToggleButton();

            setManagedProperties(
                /* allowRoaming= */ {activeValue: false},
                /* roamingState= */ null);

            assertEquals(
                cellularRoamingToggleButton.i18n(
                    'networkAllowDataRoamingDisabled'),
                cellularRoamingToggleButton.getSubLabelForTesting());

            setManagedProperties(
                /* allowRoaming= */ {activeValue: true},
                /* roamingState= */ 'Home');

            assertEquals(
                cellularRoamingToggleButton.i18n(
                    'networkAllowDataRoamingEnabledHome'),
                cellularRoamingToggleButton.getSubLabelForTesting());

            setManagedProperties(
                /* allowRoaming= */ {activeValue: true},
                /* roamingState= */ 'Roaming');

            assertEquals(
                cellularRoamingToggleButton.i18n(
                    'networkAllowDataRoamingEnabledRoaming'),
                cellularRoamingToggleButton.getSubLabelForTesting());

            prefs_.cros.signed.data_roaming_enabled.value = false;
            cellularRoamingToggleButton.prefs = Object.assign({}, prefs_);

            assertEquals(
                cellularRoamingToggleButton.i18n(
                    'networkAllowDataRoamingDisabled'),
                cellularRoamingToggleButton.getSubLabelForTesting());
          }));

  suite('Cellular per-network roaming disabled', function() {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        allowPerNetworkRoaming: false,
      });
    });

    setup(function() {
      createCellularRoamingToggleButton();
    });

    test('Toggle controls preference', async function() {
      const cellularRoamingToggle =
          cellularRoamingToggleButton.getCellularRoamingToggle();

      assertTrue(cellularRoamingToggle.checked);
      assertTrue(cellularRoamingToggleButton.prefs.cros.signed
                     .data_roaming_enabled.value);

      cellularRoamingToggle.click();

      assertFalse(cellularRoamingToggle.checked);
      assertFalse(cellularRoamingToggleButton.prefs.cros.signed
                      .data_roaming_enabled.value);
    });
  });

  suite('Cellular per-network roaming enabled', function() {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        allowPerNetworkRoaming: true,
      });
    });

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
