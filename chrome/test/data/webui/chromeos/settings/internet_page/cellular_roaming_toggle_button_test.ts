// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://webui-test/chromeos/network/cr_policy_strings.js';

import {CellularRoamingToggleButtonElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement} from 'chrome://os-settings/os_settings.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ManagedBoolean} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<cellular-roaming-toggle-button>', () => {
  let cellularRoamingToggleButton: CellularRoamingToggleButtonElement;
  let mojoApi_: FakeNetworkConfig;

  interface EnforcementCase {
    controlledBy: chrome.settingsPrivate.ControlledBy;
    managedPropertiesPolicySource: PolicySource;
    managedPropertiesPolicyValue: boolean;
    managedPropertiesActiveValue: boolean;
    prefValue: boolean;
  }

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

  const managedBoolean: ManagedBoolean = {
    activeValue: false,
    policySource: PolicySource.kNone,
    policyValue: false,
  };

  async function createCellularRoamingToggleButton(): Promise<void> {
    cellularRoamingToggleButton =
        document.createElement('cellular-roaming-toggle-button');

    const allowRoaming = {...managedBoolean};
    setManagedProperties(
        /* allowRoaming= */ allowRoaming, /* roamingState= */ '');

    cellularRoamingToggleButton.prefs = Object.assign({}, prefs_);
    document.body.appendChild(cellularRoamingToggleButton);
    await flushTasks();
  }

  async function setManagedProperties(
      allowRoaming: ManagedBoolean, roamingState: string): Promise<void> {
    const props = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    props.typeProperties.cellular!.allowRoaming = allowRoaming;
    props.typeProperties.cellular!.roamingState = roamingState;

    cellularRoamingToggleButton.set('managedProperties', props);
    mojoApi_.setManagedPropertiesForTest(props);
    await flushTasks();
  }

  /**
   * Generates and returns a list of test cases that tests covering policy
   * enforcement of roaming should cover.
   */
  function getAllowRoamingEnforcementTestCases(): EnforcementCase[] {
    const ControlledBy = chrome.settingsPrivate.ControlledBy;

    const enforcementCases: EnforcementCase[] = [];

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
              const enforcement: EnforcementCase = {
                controlledBy,
                managedPropertiesPolicySource,
                managedPropertiesPolicyValue,
                managedPropertiesActiveValue,
                prefValue,
              };
              enforcementCases.push(enforcement);
            }
          }
        }
      }
    }
    return enforcementCases;
  }

  function getRoamingToggleButtonSubLabelText(): string {
    const allowRoamingToggle =
        cellularRoamingToggleButton.getCellularRoamingToggle();
    if (!allowRoamingToggle) {
      return '';
    }
    const subLabel =
        allowRoamingToggle.shadowRoot!.querySelector<HTMLElement>('#sub-label');
    if (!subLabel) {
      return '';
    }
    return subLabel.innerText;
  }

  function getRoamingTogglePolicyIndicatorText(): string {
    const allowRoamingToggle =
        cellularRoamingToggleButton.getCellularRoamingToggle();
    if (!allowRoamingToggle) {
      return '';
    }
    const policyIndicator =
        allowRoamingToggle.shadowRoot!
            .querySelector('cr-policy-network-indicator-mojo');
    if (!policyIndicator) {
      return '';
    }
    return policyIndicator.get('indicatorTooltip_');
  }

  function policyIconIsVisible(cellularRoamingToggle: CrToggleElement):
      boolean {
    if (!cellularRoamingToggle) {
      return false;
    }
    const policyIndicator = cellularRoamingToggle.shadowRoot!.querySelector(
        'cr-policy-network-indicator-mojo');
    if (!policyIndicator) {
      return false;
    }
    const policyIcon =
        policyIndicator.shadowRoot!.querySelector('cr-tooltip-icon');
    return isVisible(policyIcon);
  }

  function isRoamingProhibitedByPolicy(): boolean {
    const dataRoamingEnabled = prefs_.cros.signed.data_roaming_enabled;
    return !dataRoamingEnabled.value &&
        dataRoamingEnabled.controlledBy ===
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
  }

  function isPerNetworkToggleDisabled(): boolean {
    return cellularRoamingToggleButton.shadowRoot!.querySelector(
        'network-config-toggle')!.disabled;
  }

  suiteSetup(() => {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi_);
  });

  setup(() => {
    prefs_.cros.signed.data_roaming_enabled.value = true;
  });

  test('Cellular roaming subtext', () => {
    createCellularRoamingToggleButton();

    const allowedRoaming = {...managedBoolean};

    // Regardless of whether roaming is enabled or not, the subtext should
    // notify the user if roaming is required by the provider.
    for (const allowRoaming of [true, false]) {
      allowedRoaming.activeValue = allowRoaming;
      setManagedProperties(
          /* allowRoaming= */ allowedRoaming,
          /* roamingState= */ 'Required');

      assertEquals(
          cellularRoamingToggleButton.i18n('networkAllowDataRoamingRequired'),
          getRoamingToggleButtonSubLabelText());
    }

    // Regardless of the roaming state, except when roaming is required, the
    // subtext should notify the user that roaming is disabled when applicable.
    for (const roamingState of ['Home', 'Roaming']) {
      allowedRoaming.activeValue = false;
      setManagedProperties(
          /* allowRoaming= */ allowedRoaming,
          /* roamingState= */ roamingState);

      assertEquals(
          cellularRoamingToggleButton.i18n('networkAllowDataRoamingDisabled'),
          getRoamingToggleButtonSubLabelText());
    }

    allowedRoaming.activeValue = true;
    // Roaming is allowed but we are not roaming.
    setManagedProperties(
        /* allowRoaming= */ allowedRoaming,
        /* roamingState= */ 'Home');

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingEnabledHome'),
        getRoamingToggleButtonSubLabelText());

    // Roaming is allowed and we are roaming.
    allowedRoaming.activeValue = true;
    setManagedProperties(
        /* allowRoaming= */ allowedRoaming,
        /* roamingState= */ 'Roaming');

    assertEquals(
        cellularRoamingToggleButton.i18n(
            'networkAllowDataRoamingEnabledRoaming'),
        getRoamingToggleButtonSubLabelText());

    // Simulate disabling roaming via policy.
    prefs_.cros.signed.data_roaming_enabled.value = false;
    cellularRoamingToggleButton.prefs = {...prefs_};

    assertEquals(
        cellularRoamingToggleButton.i18n('networkAllowDataRoamingDisabled'),
        getRoamingToggleButtonSubLabelText());
  });

  suite('Cellular per-network roaming', () => {
    setup(() => {
      mojoApi_.resetForTest();
      createCellularRoamingToggleButton();
    });

    test('Toggle controls property', async () => {
      const cellularRoamingToggle =
          cellularRoamingToggleButton.getCellularRoamingToggle();
      assertTrue(!!cellularRoamingToggle);

      assertFalse(cellularRoamingToggle.checked);
      assertFalse(
          cellularRoamingToggleButton.get('isRoamingAllowedForNetwork_'));

      cellularRoamingToggle.click();

      await mojoApi_.whenCalled('setProperties');

      assertTrue(cellularRoamingToggle.checked);
      assertTrue(
          cellularRoamingToggleButton.get('isRoamingAllowedForNetwork_'));
    });

    test('Toggle is influenced by policy', async () => {
      const cellularRoamingToggle =
          cellularRoamingToggleButton.getCellularRoamingToggle();
      assertTrue(!!cellularRoamingToggle);

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
        cellularRoamingToggleButton.prefs = {
          ...prefs_,
        };

        await flushTasks();

        if (!enforcementCase.prefValue &&
            enforcementCase.controlledBy === ControlledBy.DEVICE_POLICY) {
          assertTrue(isRoamingProhibitedByPolicy());
          assertTrue(isPerNetworkToggleDisabled());
          assertFalse(
              cellularRoamingToggleButton.get('isRoamingAllowedForNetwork_'));
          assertFalse(cellularRoamingToggle.checked);
          assertTrue(policyIconIsVisible(cellularRoamingToggle));
          assertEquals(
              window.CrPolicyStrings.controlledSettingPolicy,
              getRoamingTogglePolicyIndicatorText());
          continue;
        }

        if (enforcementCase.managedPropertiesPolicySource ===
                PolicySource.kUserPolicyEnforced ||
            enforcementCase.managedPropertiesPolicySource ===
                PolicySource.kDevicePolicyEnforced) {
          assertFalse(isRoamingProhibitedByPolicy());
          assertTrue(isPerNetworkToggleDisabled());
          assertEquals(
              enforcementCase.managedPropertiesPolicyValue,
              cellularRoamingToggleButton.get('isRoamingAllowedForNetwork_'));
          assertEquals(
              enforcementCase.managedPropertiesPolicyValue,
              cellularRoamingToggle.checked);
          assertTrue(policyIconIsVisible(cellularRoamingToggle));

          switch (enforcementCase.managedPropertiesPolicySource) {
            case PolicySource.kUserPolicyEnforced:
            case PolicySource.kDevicePolicyEnforced:
              assertEquals(
                  window.CrPolicyStrings.controlledSettingPolicy,
                  getRoamingTogglePolicyIndicatorText());
              break;
            case PolicySource.kUserPolicyRecommended:
            case PolicySource.kDevicePolicyRecommended:
              assertEquals(
                  (enforcementCase.managedPropertiesActiveValue ===
                   enforcementCase.managedPropertiesPolicyValue) ?
                      window.CrPolicyStrings
                          .controlledSettingRecommendedMatches :
                      window.CrPolicyStrings
                          .controlledSettingRecommendedDiffers,
                  getRoamingTogglePolicyIndicatorText());
              break;
          }
          continue;
        }

        assertFalse(isRoamingProhibitedByPolicy());
        assertFalse(
            cellularRoamingToggle.shadowRoot!.querySelector(
                                                 'cr-toggle')!.disabled);
        assertEquals(
            enforcementCase.managedPropertiesActiveValue,
            cellularRoamingToggleButton.get('isRoamingAllowedForNetwork_'));
        assertEquals(
            enforcementCase.managedPropertiesActiveValue,
            cellularRoamingToggle.checked);
        assertFalse(policyIconIsVisible(cellularRoamingToggle));
      }
    });
  });
});
