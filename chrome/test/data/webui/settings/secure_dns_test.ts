// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns and
 * secure-dns-input.
 */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SecureDnsInputElement, SettingsSecureDnsElement, SettingsToggleButtonElement} from 'chrome://settings/lazy_load.js';
import {SecureDnsResolverType} from 'chrome://settings/lazy_load.js';
import type {ResolverOption} from 'chrome://settings/settings.js';
import {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// <if expr="chromeos_ash">
import type {SettingsSecureDnsDialogElement} from 'chrome://settings/lazy_load.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

// </if>

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

suite('SettingsSecureDnsInput', function() {
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

suite('SettingsSecureDns', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsSecureDnsElement;
  let secureDnsToggle: SettingsToggleButtonElement;

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
    assertTrue(secureDnsToggle.checked);
    assertFalse(secureDnsToggle.$.control.disabled);
    assertFalse(testElement.$.resolverSelect.hidden);
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showSecureDnsSetting: true,
      secureDnsDescription: defaultDescription,
      secureDnsDisabledForManagedEnvironment: managedEnvironmentDescription,
      secureDnsDisabledForParentalControl: parentalControlDescription,
      isRevampWayfindingEnabled: false,
    });
  });

  setup(async function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    testBrowserProxy.setResolverList(resolverList);
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-secure-dns');
    testElement.prefs = {
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(testElement);

    await testBrowserProxy.whenCalled('getSecureDnsSetting');
    await flushTasks();

    secureDnsToggle =
        testElement.shadowRoot!.querySelector('#secureDnsToggle')!;
    assertTrue(isVisible(secureDnsToggle));

    assertResolverSelectShown();
    assertEquals(
        SecureDnsResolverType.AUTOMATIC, testElement.$.resolverSelect.value);
  });

  teardown(function() {
    testElement.remove();
  });

  function getResolverOptions(): HTMLElement {
    const options =
        testElement.shadowRoot!.querySelector<HTMLElement>('#resolverOptions');
    assertTrue(!!options);
    return options;
  }

  test('SecureDnsOff', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertFalse(secureDnsToggle.$.control.disabled);
    assertTrue(getResolverOptions().hidden);
    assertEquals(
        'none',
        getComputedStyle(testElement.$.secureDnsInputContainer).display);
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
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
    assertResolverSelectShown();
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(!!secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertEquals(
        SecureDnsResolverType.AUTOMATIC, testElement.$.resolverSelect.value);
  });

  test('SecureDnsSecure', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: resolverList[0]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertResolverSelectShown();
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(!!secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertEquals('0', testElement.$.resolverSelect.value);
  });

  test('SecureDnsManagedEnvironment', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.DISABLED_MANAGED,
    });
    flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.$.control.disabled);
    assertTrue(getResolverOptions().hidden);
    assertEquals(managedEnvironmentDescription, secureDnsToggle.subLabel);
    assertTrue(!!secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertTrue(
        secureDnsToggle.shadowRoot!.querySelector('cr-policy-pref-indicator')!
            .shadowRoot!.querySelector('cr-tooltip-icon')!.hidden);
  });

  test('SecureDnsParentalControl', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.DISABLED_PARENTAL_CONTROLS,
    });
    flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.$.control.disabled);
    assertTrue(getResolverOptions().hidden);
    assertEquals(parentalControlDescription, secureDnsToggle.subLabel);
    assertTrue(!!secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertTrue(
        secureDnsToggle.shadowRoot!.querySelector('cr-policy-pref-indicator')!
            .shadowRoot!.querySelector('cr-tooltip-icon')!.hidden);
  });

  test('SecureDnsManaged', function() {
    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;

    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertTrue(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.$.control.disabled);
    assertTrue(getResolverOptions().hidden);
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertTrue(!!secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertFalse(
        secureDnsToggle.shadowRoot!.querySelector('cr-policy-pref-indicator')!
            .shadowRoot!.querySelector('cr-tooltip-icon')!.hidden);
  });

  // <if expr="chromeos_ash">
  test('SecureDnsManagedWithIdentifiers', function() {
    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;

    const effectiveConfig = 'https://example/dns-query';
    const displayConfig = 'https://example-for-display/dns-query';

    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: effectiveConfig,
      dohWithIdentifiersActive: true,
      configForDisplay: displayConfig,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const expectedDescription = loadTimeData.substituteString(
        loadTimeData.getString('secureDnsWithIdentifiersDescription'),
        displayConfig);
    assertEquals(expectedDescription, secureDnsToggle.subLabel);
  });
  // </if>
});

// <if expr="chromeos_ash">
suite('OsSettingsRevampSecureDnsDialog', () => {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsSecureDnsElement;
  let secureDnsToggle: SettingsToggleButtonElement;
  let secureDnsToggleDialog: SettingsSecureDnsDialogElement;

  /**
   * Checks that the select menu is shown and the toggle is properly
   * configured for showing the configuration options.
   */
  function assertResolverSelectShown() {
    assertTrue(secureDnsToggle.checked);
    assertFalse(testElement.$.resolverSelect.hidden);
  }

  function setAndAssertSecureDnsDialog() {
    secureDnsToggleDialog =
        testElement.shadowRoot!.querySelector('#warningDialog')!;
    assertTrue(!!secureDnsToggleDialog);
    assertTrue(isVisible(secureDnsToggleDialog.$.cancelButton));
    assertTrue(isVisible(secureDnsToggleDialog.$.disableButton));
  }

  function getResolverOptions(): HTMLElement {
    const options =
        testElement.shadowRoot!.querySelector<HTMLElement>('#resolverOptions');
    assertTrue(!!options);
    return options;
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showSecureDnsSetting: true,
      isRevampWayfindingEnabled: true,
    });
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    testElement = document.createElement('settings-secure-dns');
    testElement.prefs = {
      'dns_over_https': {
        'mode': {'value': SecureDnsMode.AUTOMATIC},
      },
    };
    document.body.appendChild(testElement);

    await testBrowserProxy.whenCalled('getSecureDnsSetting');
    await flushTasks();

    secureDnsToggle =
        testElement.shadowRoot!.querySelector('#secureDnsToggle')!;
    assertTrue(isVisible(secureDnsToggle));

    assertResolverSelectShown();
    assertEquals(
        SecureDnsResolverType.AUTOMATIC, testElement.$.resolverSelect.value);
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsDialogSanityCheck', () => {
    // Initiate a toggle change from on to off, opens the warning dialog.
    secureDnsToggle.click();
    flush();

    setAndAssertSecureDnsDialog();
  });

  test('SecureDnsDialogCancel', async () => {
    // Initiate a toggle change from on to off, opens the warning dialog.
    secureDnsToggle.click();
    flush();
    setAndAssertSecureDnsDialog();

    secureDnsToggleDialog.$.cancelButton.click();
    flush();

    // Wait for onDisableDnsDialogClosed_ to finish.
    await flushTasks();
    await waitAfterNextRender(secureDnsToggle);

    assertFalse(secureDnsToggleDialog.$.dialog.open);
    assertTrue(secureDnsToggle.checked);
    assertResolverSelectShown();
    assertEquals(
        SecureDnsResolverType.AUTOMATIC, testElement.$.resolverSelect.value);
  });

  test('SecureDnsDialogOnToOff', () => {
    // Initiate a toggle change from on to off, opens the warning dialog.
    secureDnsToggle.click();
    flush();
    setAndAssertSecureDnsDialog();

    // Turn off the toggle
    secureDnsToggleDialog.$.disableButton.click();
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();

    assertFalse(secureDnsToggle.checked);
    assertTrue(getResolverOptions().hidden);
  });

  test('SecureDnsDialogSecureOffToOn', () => {
    // If the user selects Custom Secure mode with an invalid input, we will not
    // register that the user wants to use secure mode (see the comment on
    // secure_dns_dialog.ts), however, when toggled off and on, we will still
    // show that the user had selected Custom before.

    // Select Secure in the menu button with no input and config.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    testElement.prefs = {
      'dns_over_https':
          {'mode': {'value': SecureDnsMode.SECURE}, 'templates': {'value': ''}},
    };

    // Simulate that the toggle is off.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    testElement.prefs = {
      'dns_over_https':
          {'mode': {'value': SecureDnsMode.OFF}, 'templates': {'value': ''}},
    };

    // Turn on the toggle
    secureDnsToggle.click();
    flush();
    assertTrue(secureDnsToggle.checked);
    assertResolverSelectShown();
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);

    // Turn off the toggle, this will dispatch an event from the dialog since
    // the invalid Custom secure mode was not registered to the pref. For more
    // info, see the comment in secure_dns_dialog.ts.
    secureDnsToggle.click();
    flush();
    setAndAssertSecureDnsDialog();

    secureDnsToggleDialog.$.disableButton.click();
    flush();
    assertFalse(secureDnsToggle.checked);
    assertTrue(getResolverOptions().hidden);

    // Turn on the toggle. The selected menu option should still be secure
    // mode.
    secureDnsToggle.click();
    flush();
    assertTrue(secureDnsToggle.checked);
    assertResolverSelectShown();
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);
  });
});
// </if>
