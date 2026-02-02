// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns-v2 and
 * secure-dns-input.
 */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SecureDnsInputElement, SettingsSecureDnsV2Element, SecurityPageFeatureRowElement} from 'chrome://settings/lazy_load.js';
import {SecureDnsV2ResolverType} from 'chrome://settings/lazy_load.js';
import type {ResolverOption, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {PrivacyPageBrowserProxyImpl, loadTimeData, SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
import {assertEquals, assertNotEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

suite('SettingsSecureDnsV2Input', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SecureDnsInputElement;

  // Possible error messages
  const invalidFormat = 'invalid format description';
  const probeFail = 'probe fail description';

  const invalidEntry = 'invalid_entry';
  const validFailEntry = 'https://example.server/dns-query';
  const validSuccessEntry = 'https://example.server.another/dns-query';

  suiteSetup(function() {
    loadTimeData.overrideValues({
      secureDnsCustomFormatError: invalidFormat,
      secureDnsCustomConnectionError: probeFail,
    });
  });

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('secure-dns-input');
    document.body.appendChild(testElement);
    flush();
    assertFalse(testElement.$.input.invalid);
    assertEquals('', testElement.value);
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsInputEmpty', async function() {
    // Trigger validation on an empty input.
    testBrowserProxy.setIsValidConfigResult('', false);
    testElement.validate();
    assertEquals('', await testBrowserProxy.whenCalled('isValidConfig'));
    assertFalse(testElement.$.input.invalid);
  });

  test('SecureDnsInputValidFormatAndProbeFail', async function() {
    // Enter a valid server that fails the test query.
    testElement.value = validFailEntry;
    testBrowserProxy.setIsValidConfigResult(validFailEntry, true);
    testBrowserProxy.setProbeConfigResult(validFailEntry, false);
    testElement.validate();
    assertEquals(
        validFailEntry, await testBrowserProxy.whenCalled('isValidConfig'));
    assertEquals(
        validFailEntry, await testBrowserProxy.whenCalled('probeConfig'));
    assertTrue(testElement.$.input.invalid);
    assertEquals(probeFail, testElement.$.input.firstFooter);
  });

  test('SecureDnsInputValidFormatAndProbeSuccess', async function() {
    // Enter a valid input and make the test query succeed.
    testElement.value = validSuccessEntry;
    testBrowserProxy.setIsValidConfigResult(validSuccessEntry, true);
    testBrowserProxy.setProbeConfigResult(validSuccessEntry, true);
    testElement.validate();
    assertEquals(
        validSuccessEntry, await testBrowserProxy.whenCalled('isValidConfig'));
    assertEquals(
        validSuccessEntry, await testBrowserProxy.whenCalled('probeConfig'));
    assertFalse(testElement.$.input.invalid);
  });

  test('SecureDnsInputInvalid', async function() {
    // Enter an invalid input and trigger validation.
    testElement.value = invalidEntry;
    testBrowserProxy.setIsValidConfigResult(invalidEntry, false);
    testElement.validate();
    assertEquals(
        invalidEntry, await testBrowserProxy.whenCalled('isValidConfig'));
    assertEquals(0, testBrowserProxy.getCallCount('probeConfig'));
    assertTrue(testElement.$.input.invalid);
    assertEquals(invalidFormat, testElement.$.input.firstFooter);

    // Trigger an input event and check that the error clears.
    testElement.$.input.dispatchEvent(new CustomEvent('input'));
    assertFalse(testElement.$.input.invalid);
    assertEquals(invalidEntry, testElement.value);
  });
});

suite('SettingsSecureDnsV2', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsSecureDnsV2Element;
  let secureDnsToggle: SecurityPageFeatureRowElement;

  const resolverList: ResolverOption[] = [
    {name: 'Resolver 1', value: 'resolver', policy: ''},
  ];

  // Possible subtitle overrides.
  const defaultDescription = 'default description';
  const managedEnvironmentDescription =
      'disabled for managed environment description';
  const parentalControlDescription =
      'disabled for parental control description';

  /**
   * Checks that the select menu is shown and the toggle is properly
   * configured for showing the configuration options.
   */
  function assertResolverSelectShown() {
    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);
    assertTrue(toggleButton.checked);
    assertFalse(toggleButton.$.control.disabled);
    assertFalse(testElement.$.resolverSelect.hidden);
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showSecureDnsSetting: true,
      secureDnsDescription: defaultDescription,
      secureDnsDisabledForManagedEnvironment: managedEnvironmentDescription,
      secureDnsDisabledForParentalControl: parentalControlDescription,
    });
  });

  setup(async function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    testBrowserProxy.setResolverList(resolverList);
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-secure-dns-v2');
    testElement.prefs = {
      dns_over_https: {
        mode: {
          value: SecureDnsMode.AUTOMATIC,
          type: chrome.settingsPrivate.PrefType.STRING,
        },
        templates: {
          value: '',
          type: chrome.settingsPrivate.PrefType.STRING,
        },
        automatic_mode_fallback_to_doh: {
          value: false,
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
        },
      },
    };
    document.body.appendChild(testElement);

    await testBrowserProxy.whenCalled('getSecureDnsSetting');
    await flushTasks();

    secureDnsToggle = testElement.$.featureRow;


    // Expand the row.
    const expandButton = secureDnsToggle.$.expandButton;
    expandButton.click();
    await flushTasks();
    assertTrue(isVisible(secureDnsToggle));

    assertResolverSelectShown();
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsOff', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);

    // Make sure toggle button is not on.
    assertFalse(toggleButton.checked);
    assertFalse(toggleButton.$.control.disabled);

    // Make sure no policy is enabled.
    assertFalse(!!secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
  });

  test('SecureDnsAutomatic', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);

    // Make sure toggle button off.
    assertTrue(toggleButton.checked);
    assertFalse(toggleButton.$.control.disabled);

    // Make sure automatic radio button is selected.
    const automaticRadioButton = testElement.$.automaticRadioButton;
    assertTrue(automaticRadioButton.checked);
    assertEquals(SecureDnsV2ResolverType.AUTOMATIC, automaticRadioButton.name);
  });

  test('SecureDnsFallback', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    testElement.setPrefValue(
        'dns_over_https.automatic_mode_fallback_to_doh', true);
    await flushTasks();
    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);

    // Make sure toggle button is on.
    assertTrue(toggleButton.checked);
    assertFalse(toggleButton.$.control.disabled);

    // Make sure fallback radio button is selected.
    const fallbackRadioButton = testElement.$.fallbackRadioButton;
    assertTrue(fallbackRadioButton.checked);
    assertEquals(SecureDnsV2ResolverType.FALLBACK, fallbackRadioButton.name);
  });

  test('SecureDnsSecureMode', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: resolverList[0]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();
    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);

    // Make sure toggle button is on.
    assertTrue(toggleButton.checked);
    assertFalse(toggleButton.$.control.disabled);

    // Make sure custom radio button is selected.
    const customRadioButton = testElement.$.customRadioButton;
    assertTrue(customRadioButton.checked);
    assertEquals(SecureDnsV2ResolverType.CUSTOM, customRadioButton.name);
  });

  test('SecureDnsToggleOffResetsSelection', async function() {
    // Start with "Custom" (Secure) mode selected.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: 'https://custom.dns',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);
    assertTrue(toggleButton.checked);

    const customRadioButton = testElement.$.customRadioButton;
    assertTrue(customRadioButton.checked);

    // Turn the toggle OFF.
    toggleButton.click();
    await flushTasks();
    assertFalse(toggleButton.checked);

    // Verify that the selection has reset to "Automatic".
    assertFalse(customRadioButton.checked);
    const automaticRadioButton = testElement.$.automaticRadioButton;
    assertTrue(automaticRadioButton.checked);

    // Verify the underlying pref is OFF.
    assertEquals(
        SecureDnsMode.OFF, testElement.getPref('dns_over_https.mode').value);
  });

  test('SecureDnsWarningIcon', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();
    assertFalse(
        secureDnsToggle.iconVisible,
        'The icon should not be visible, Secure DNS is ON');

    // Switch to OFF (without enforcement).
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();
    assertTrue(secureDnsToggle.iconVisible, 'The icon should be visible');

    // Switch to OFF by enforcement.
    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      // TODO(crbug.com/440389379): Remove or update this, `managementMode` is
      // currently unused by the element.
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();
    assertFalse(
        secureDnsToggle.iconVisible,
        'The icon should not be visible, a policy set Secure DNS to OFF');
    // TODO(crbug.com/441316657): Add a check for the policy indicator icon.
  });

  test('SecureDnsWarningIconWithManagementMode', async function() {
    // Initial state: OFF, no override. Icon should be visible.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();
    assertTrue(secureDnsToggle.iconVisible, 'The icon should be visible');

    // Switch to OFF with DISABLED_MANAGED.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.DISABLED_MANAGED,
    });
    await flushTasks();
    assertFalse(
        secureDnsToggle.iconVisible,
        'The icon should not be visible when disabled in managed environment');

    // Switch to OFF with DISABLED_PARENTAL_CONTROLS.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.DISABLED_PARENTAL_CONTROLS,
    });
    await flushTasks();
    assertFalse(
        secureDnsToggle.iconVisible,
        'The icon should not be visible when disabled with parental controls');
  });

  test('RadioButtonsDisabledWhenEnforced', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    assertNotEquals(
        testElement.getPref('dns_over_https.mode').enforcement,
        chrome.settingsPrivate.Enforcement.ENFORCED);
    assertFalse(testElement.$.automaticRadioButton.disabled);
    assertFalse(testElement.$.fallbackRadioButton.disabled);
    assertFalse(testElement.$.customRadioButton.disabled);

    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    assertEquals(
        testElement.getPref('dns_over_https.mode').enforcement,
        chrome.settingsPrivate.Enforcement.ENFORCED);
    assertTrue(testElement.$.automaticRadioButton.disabled);
    assertTrue(testElement.$.fallbackRadioButton.disabled);
    assertTrue(testElement.$.customRadioButton.disabled);

    testElement.prefs.dns_over_https.mode.enforcement = null;
    testElement.prefs.dns_over_https.mode.controlledBy = null;
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    assertNotEquals(
        testElement.getPref('dns_over_https.mode').enforcement,
        chrome.settingsPrivate.Enforcement.ENFORCED);
    assertFalse(testElement.$.automaticRadioButton.disabled);
    assertFalse(testElement.$.fallbackRadioButton.disabled);
    assertFalse(testElement.$.customRadioButton.disabled);
  });

  test('SecureDnsFeatureStateStrings', async function() {
    // Collapse row to see feature state string.
    secureDnsToggle.$.expandButton.click();
    await flushTasks();

    const stateOff = loadTimeData.getString('securityFeatureRowStateOff');
    const stateStandard =
        loadTimeData.getString('securityFeatureRowStateStandard');
    const stateEnhanced =
        loadTimeData.getString('securityFeatureRowStateEnhanced');
    const stateEnhancedCustom =
        loadTimeData.getString('securityFeatureRowStateEnhancedCustom');

    // Off toggle.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    let label = secureDnsToggle.shadowRoot!.querySelector('#stateLabel');
    assertTrue(!!label);
    assertEquals(stateOff, label.textContent.trim());

    // Standard Radio Button.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    label = secureDnsToggle.shadowRoot!.querySelector('#stateLabel');
    assertTrue(!!label);
    assertEquals(stateStandard, label.textContent.trim());

    // Fallback Radio Button.
    testElement.setPrefValue(
        'dns_over_https.automatic_mode_fallback_to_doh', true);

    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    label = secureDnsToggle.shadowRoot!.querySelector('#stateLabel');
    assertTrue(!!label);
    assertEquals(stateEnhanced, label.textContent.trim());

    // Custom Radio Button.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: 'https://custom.dns',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    await flushTasks();

    label = secureDnsToggle.shadowRoot!.querySelector('#stateLabel');
    assertTrue(!!label);
    assertEquals(stateEnhancedCustom, label.textContent.trim());
  });
});
