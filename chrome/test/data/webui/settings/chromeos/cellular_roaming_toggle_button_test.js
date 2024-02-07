// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://webui-test/cr_components/chromeos/network/cr_policy_strings.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

suite('CellularRoamingToggleButton', function() {
  /** @type {CellularRoamingToggleButton|undefined} */
  let cellularRoamingToggleButton;

  /** @type {?CrosNetworkConfigRemote} */
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
      type: NetworkType.kCellular,
      typeProperties: {
        cellular: {
          allowRoaming: allowRoaming,
          roamingState: roamingState,
        },
      },
    };
    flush();
  }

  /**
   * @return {Array} Generates and returns a list of test cases that tests
   * covering policy enforcement of roaming should cover.
   */
  function getAllowRoamingEnforcementTestCases() {
    const ControlledBy = chrome.settingsPrivate.ControlledBy;

    const enforcementCases = [];

    // The enforcement of the cellular roaming pref a.k.a. the global policy.
    for (const controlledBy
             of [ControlledBy.OWNER, ControlledBy.DEVICE_POLICY]) {
      // The source of the policy affecting roaming for the particular network.
      for (const managedPropertiesPolicySource
               of [PolicySource.kNone, PolicySource.kUserPolicyEnforced,
                   PolicySource.kDevicePolicyEnforced]) {
        // The value of the cellular roaming pref.
        for (const prefValue of [true, false]) {
          // The value of the policy affecting roaming for the particular
          // network.
          for (const managedPropertiesPolicyValue of [true, false]) {
            // The effective value of roaming for the particular network, e.g.
            // whether it is enabled or disabled.
            for (const managedPropertiesActiveValue of [true, false]) {
              enforcementCases.push({
                controlledBy: controlledBy,
                managedPropertiesPolicySource: managedPropertiesPolicySource,
                managedPropertiesPolicyValue: managedPropertiesPolicyValue,
                managedPropertiesActiveValue: managedPropertiesActiveValue,
                prefValue: prefValue,
              });
            }
          }
        }
      }
    }
    return enforcementCases;
  }

  function getRoamingToggleButtonSubLabelText() {
    const allowRoamingToggle =
        cellularRoamingToggleButton.getCellularRoamingToggle();
    if (!allowRoamingToggle) {
      return '';
    }
    const subLabel = allowRoamingToggle.shadowRoot.querySelector('#sub-label');
    if (!subLabel) {
      return '';
    }
    return subLabel.innerText;
  }

  function getRoamingTogglePolicyIndicatorText() {
    const allowRoamingToggle =
        cellularRoamingToggleButton.getCellularRoamingToggle();
    if (!allowRoamingToggle) {
      return '';
    }
    const policyIndicator = allowRoamingToggle.shadowRoot.querySelector(
        'cr-policy-network-indicator-mojo');
    if (!policyIndicator) {
      return '';
    }
    return policyIndicator.indicatorTooltip_;
  }

  function policyIconIsVisible(cellularRoamingToggle) {
    if (!cellularRoamingToggle) {
      return false;
    }
    const policyIndicator = cellularRoamingToggle.shadowRoot.querySelector(
        'cr-policy-network-indicator-mojo');
    if (!policyIndicator) {
      return false;
    }
    const policyIcon =
        policyIndicator.shadowRoot.querySelector('cr-tooltip-icon');
    return !!policyIcon && !policyIcon.hidden;
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
          getRoamingToggleButtonSubLabelText());
    }

    // Regardless of the roaming state, except when roaming is required, the
    // subtext should notify the user that roaming is disabled when applicable.
    for (const roamingState in ['Home', 'Roaming']) {
      setManagedProperties(
          /* allowRoaming= */ {activeValue: false},
          /* roamingState= */ roamingState);

      assertEquals(
          cellularRoamingToggleButton.i18n('networkAllowDataRoamingDisabled'),
          getRoamingToggleButtonSubLabelText());
    }

    // Roaming is allowed but we are not roaming.
    setManagedProperties(
        /* allowRoaming= */ {activeValue: true},
        /* roamingState= */ 'Home');

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingEnabledHome'),
        getRoamingToggleButtonSubLabelText());

    // Roaming is allowed and we are roaming.
    setManagedProperties(
        /* allowRoaming= */ {activeValue: true},
        /* roamingState= */ 'Roaming');

    assertEquals(
        cellularRoamingToggleButton.i18n(
            'networkAllowDataRoamingEnabledRoaming'),
        getRoamingToggleButtonSubLabelText());

    // Simulate disabling roaming via policy.
    prefs_.cros.signed.data_roaming_enabled.value = false;
    cellularRoamingToggleButton.prefs = Object.assign({}, prefs_);

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingDisabled'),
        getRoamingToggleButtonSubLabelText());
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

    test('Toggle is influenced by policy', function() {
      const cellularRoamingToggle =
          cellularRoamingToggleButton.getCellularRoamingToggle();

      const ControlledBy = chrome.settingsPrivate.ControlledBy;

      for (const enforcementCase of getAllowRoamingEnforcementTestCases()) {
        // There is not a case where the value provided for the pref will be
        // |false| except when enforced as a device policy.
        if (!enforcementCase.prefValue &&
            enforcementCase.controlledBy !== ControlledBy.DEVICE_POLICY) {
          continue;
        }

        // There is not a case where policy is enforcing a value and the active
        // and policy values will not be equal.
        if (enforcementCase.controlledBy === ControlledBy.DEVICE_POLICY ||
            enforcementCase.managedPropertiesPolicySource ===
                PolicySource.kUserPolicyEnforced ||
            enforcementCase.managedPropertiesPolicySource ===
                PolicySource.kDevicePolicyEnforced) {
          if (enforcementCase.managedPropertiesActiveValue !==
              enforcementCase.managedPropertiesPolicyValue) {
            continue;
          }
        }

        setManagedProperties(
            /* allowRoaming= */ {
              activeValue: enforcementCase.managedPropertiesActiveValue,
              policySource: enforcementCase.managedPropertiesPolicySource,
              policyValue: enforcementCase.managedPropertiesPolicyValue,
            },
            /* roamingState= */ 'Home');

        prefs_.cros.signed.data_roaming_enabled.value =
            enforcementCase.prefValue;
        prefs_.cros.signed.data_roaming_enabled.controlledBy =
            enforcementCase.controlledBy;
        cellularRoamingToggleButton.prefs = Object.assign({}, prefs_);

        const dataRoamingEnabled =
            cellularRoamingToggleButton.prefs.cros.signed.data_roaming_enabled;

        flush();

        if (!enforcementCase.prefValue &&
            enforcementCase.controlledBy === ControlledBy.DEVICE_POLICY) {
          assertTrue(
              cellularRoamingToggleButton.isRoamingProhibitedByPolicy_());
          assertTrue(cellularRoamingToggleButton.isPerNetworkToggleDisabled_());
          assertFalse(cellularRoamingToggleButton.isRoamingAllowedForNetwork_);
          assertFalse(cellularRoamingToggle.checked);
          assertTrue(policyIconIsVisible(cellularRoamingToggle));
          assertEquals(
              CrPolicyStrings.controlledSettingPolicy,
              getRoamingTogglePolicyIndicatorText());
          continue;
        }

        if (enforcementCase.managedPropertiesPolicySource ===
                PolicySource.kUserPolicyEnforced ||
            enforcementCase.managedPropertiesPolicySource ===
                PolicySource.kDevicePolicyEnforced) {
          assertFalse(
              cellularRoamingToggleButton.isRoamingProhibitedByPolicy_());
          assertTrue(cellularRoamingToggleButton.isPerNetworkToggleDisabled_());
          assertEquals(
              enforcementCase.managedPropertiesPolicyValue,
              cellularRoamingToggleButton.isRoamingAllowedForNetwork_);
          assertEquals(
              enforcementCase.managedPropertiesPolicyValue,
              cellularRoamingToggle.checked);
          assertTrue(policyIconIsVisible(cellularRoamingToggle));

          switch (enforcementCase.managedPropertiesPolicySource) {
            case PolicySource.kUserPolicyEnforced:
            case PolicySource.kDevicePolicyEnforced:
              assertEquals(
                  CrPolicyStrings.controlledSettingPolicy,
                  getRoamingTogglePolicyIndicatorText());
              break;
            case PolicySource.kUserPolicyRecommended:
            case PolicySource.kDevicePolicyRecommended:
              assertEquals(
                  (enforcementCase.managedPropertiesActiveValue ===
                   enforcementCase.managedPropertiesPolicyValue) ?
                      CrPolicyStrings.controlledSettingRecommendedMatches :
                      CrPolicyStrings.controlledSettingRecommendedDiffers,
                  getRoamingTogglePolicyIndicatorText());
              break;
          }
          continue;
        }

        assertFalse(cellularRoamingToggleButton.isRoamingProhibitedByPolicy_());
        assertFalse(cellularRoamingToggle.shadowRoot.querySelector('cr-toggle')
                        .disabled);
        assertEquals(
            enforcementCase.managedPropertiesActiveValue,
            cellularRoamingToggleButton.isRoamingAllowedForNetwork_);
        assertEquals(
            enforcementCase.managedPropertiesActiveValue,
            cellularRoamingToggle.checked);
        assertFalse(policyIconIsVisible(cellularRoamingToggle));
      }
    });
  });
});
