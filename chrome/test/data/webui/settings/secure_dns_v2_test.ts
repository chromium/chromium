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
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

    // Access toggle
    secureDnsToggle =
        testElement.shadowRoot!.querySelector('#secureDnsToggle')!;
    assertTrue(!!secureDnsToggle);

    // Access expand button
    const expandButton = secureDnsToggle.$.expandButton;

    // Expand the row.
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
    flush();
    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);

    // Make sure toggle button is on.
    assertTrue(toggleButton.checked);
    assertFalse(toggleButton.$.control.disabled);

    // Make sure fallback radio button is selected.
    const fallbackRadioButton = testElement.$.fallbackRadioButton;
    fallbackRadioButton.click();
    await flushTasks();
    assertTrue(fallbackRadioButton.checked);
    assertEquals(SecureDnsV2ResolverType.FALLBACK, fallbackRadioButton.name);
  });

  test('SecureDnsSecureMode', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: resolverList[0]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const toggleButton =
        secureDnsToggle.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toggleButton');
    assertTrue(!!toggleButton);

    // Make sure toggle button is on.
    assertTrue(toggleButton.checked);
    assertFalse(toggleButton.$.control.disabled);

    // Make sure custom radio button is selected.
    const customRadioButton = testElement.$.customRadioButton;
    customRadioButton.click();
    await flushTasks();
    assertTrue(customRadioButton.checked);
    assertEquals(SecureDnsV2ResolverType.CUSTOM, customRadioButton.name);
  });
});
