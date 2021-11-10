// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns and
 * secure-dns-input.
 */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SecureDnsInputElement, SettingsSecureDnsElement} from 'chrome://settings/lazy_load.js';
import {PrivacyPageBrowserProxyImpl, ResolverOption, SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

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
    document.body.innerHTML = '';
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
    testBrowserProxy.setParsedEntry([]);
    testElement.validate();
    assertEquals('', await testBrowserProxy.whenCalled('parseCustomDnsEntry'));
    assertFalse(testElement.$.input.invalid);
    assertFalse(testElement.isInvalid());
  });

  test('SecureDnsInputValidFormatAndProbeFail', async function() {
    // Enter a valid server that fails the test query.
    testElement.value = validFailEntry;
    testBrowserProxy.setParsedEntry([validFailEntry]);
    testBrowserProxy.setProbeResults({[validFailEntry]: false});
    testElement.validate();
    assertEquals(
        validFailEntry,
        await testBrowserProxy.whenCalled('parseCustomDnsEntry'));
    assertEquals(
        validFailEntry,
        await testBrowserProxy.whenCalled('probeCustomDnsTemplate'));
    assertTrue(testElement.$.input.invalid);
    assertTrue(testElement.isInvalid());
    assertEquals(probeFail, testElement.$.input.errorMessage);
  });

  test('SecureDnsInputValidFormatAndProbeSuccess', async function() {
    // Enter a valid input and make the test query succeed.
    testElement.value = validSuccessEntry;
    testBrowserProxy.setParsedEntry([validSuccessEntry]);
    testBrowserProxy.setProbeResults({[validSuccessEntry]: true});
    testElement.validate();
    assertEquals(
        validSuccessEntry,
        await testBrowserProxy.whenCalled('parseCustomDnsEntry'));
    assertEquals(
        validSuccessEntry,
        await testBrowserProxy.whenCalled('probeCustomDnsTemplate'));
    assertFalse(testElement.$.input.invalid);
    assertFalse(testElement.isInvalid());
  });

  test('SecureDnsInputValidFormatAndProbeTwice', async function() {
    // Enter two valid servers but make the first one fail the test query.
    testElement.value = `${validFailEntry} ${validSuccessEntry}`;
    testBrowserProxy.setParsedEntry([validFailEntry, validSuccessEntry]);
    testBrowserProxy.setProbeResults({
      [validFailEntry]: false,
      [validSuccessEntry]: true,
    });
    testElement.validate();
    assertEquals(
        `${validFailEntry} ${validSuccessEntry}`,
        await testBrowserProxy.whenCalled('parseCustomDnsEntry'));
    await flushTasks();
    assertEquals(2, testBrowserProxy.getCallCount('probeCustomDnsTemplate'));
    assertFalse(testElement.$.input.invalid);
    assertFalse(testElement.isInvalid());
  });

  test('SecureDnsInputInvalid', async function() {
    // Enter an invalid input and trigger validation.
    testElement.value = invalidEntry;
    testBrowserProxy.setParsedEntry([]);
    testElement.validate();
    assertEquals(
        invalidEntry, await testBrowserProxy.whenCalled('parseCustomDnsEntry'));
    assertTrue(testElement.$.input.invalid);
    assertTrue(testElement.isInvalid());
    assertEquals(invalidFormat, testElement.$.input.errorMessage);

    // Trigger an input event and check that the error clears.
    testElement.$.input.fire('input');
    assertFalse(testElement.$.input.invalid);
    assertFalse(testElement.isInvalid());
    assertEquals(invalidEntry, testElement.value);
  });
});

suite('SettingsSecureDns', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsSecureDnsElement;

  const resolverList: ResolverOption[] = [
    {name: 'Custom', value: 'custom', policy: ''},
  ];

  // Possible subtitle overrides.
  const defaultDescription = 'default description';
  const managedEnvironmentDescription =
      'disabled for managed environment description';
  const parentalControlDescription =
      'disabled for parental control description';

  /**
   * Checks that the radio buttons are shown and the toggle is properly
   * configured for showing the radio buttons.
   */
  function assertRadioButtonsShown() {
    assertTrue(testElement.$.secureDnsToggle.hasAttribute('checked'));
    assertFalse(testElement.$.secureDnsToggle.$.control.disabled);
    assertFalse(testElement.$.secureDnsRadioGroup.hidden);
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
    document.body.innerHTML = '';
    testElement = document.createElement('settings-secure-dns');
    testElement.prefs = {
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(testElement);

    await testBrowserProxy.whenCalled('getSecureDnsSetting');
    await flushTasks();
    assertRadioButtonsShown();
    assertEquals(
        testBrowserProxy.secureDnsSetting.mode,
        testElement.$.secureDnsRadioGroup.selected);
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsOff', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertFalse(testElement.$.secureDnsToggle.hasAttribute('checked'));
    assertFalse(testElement.$.secureDnsToggle.$.control.disabled);
    assertTrue(testElement.$.secureDnsRadioGroup.hidden);
    assertEquals(defaultDescription, testElement.$.secureDnsToggle.subLabel);
    assertFalse(!!testElement.$.secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
  });

  test('SecureDnsAutomatic', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertRadioButtonsShown();
    assertEquals(defaultDescription, testElement.$.secureDnsToggle.subLabel);
    assertFalse(!!testElement.$.secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.$.secureDnsRadioGroup.selected);
  });

  test('SecureDnsSecure', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertRadioButtonsShown();
    assertEquals(defaultDescription, testElement.$.secureDnsToggle.subLabel);
    assertFalse(!!testElement.$.secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
  });

  test('SecureDnsManagedEnvironment', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      templates: [],
      managementMode: SecureDnsUiManagementMode.DISABLED_MANAGED,
    });
    flush();
    assertFalse(testElement.$.secureDnsToggle.hasAttribute('checked'));
    assertTrue(testElement.$.secureDnsToggle.$.control.disabled);
    assertTrue(testElement.$.secureDnsRadioGroup.hidden);
    assertEquals(
        managedEnvironmentDescription, testElement.$.secureDnsToggle.subLabel);
    assertTrue(!!testElement.$.secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertTrue(testElement.$.secureDnsToggle.shadowRoot!
                   .querySelector('cr-policy-pref-indicator')!.shadowRoot!
                   .querySelector('cr-tooltip-icon')!.hidden);
  });

  test('SecureDnsParentalControl', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      templates: [],
      managementMode: SecureDnsUiManagementMode.DISABLED_PARENTAL_CONTROLS,
    });
    flush();
    assertFalse(testElement.$.secureDnsToggle.hasAttribute('checked'));
    assertTrue(testElement.$.secureDnsToggle.$.control.disabled);
    assertTrue(testElement.$.secureDnsRadioGroup.hidden);
    assertEquals(
        parentalControlDescription, testElement.$.secureDnsToggle.subLabel);
    assertTrue(!!testElement.$.secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertTrue(testElement.$.secureDnsToggle.shadowRoot!
                   .querySelector('cr-policy-pref-indicator')!.shadowRoot!
                   .querySelector('cr-tooltip-icon')!.hidden);
  });

  test('SecureDnsManaged', function() {
    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;

    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertTrue(testElement.$.secureDnsToggle.hasAttribute('checked'));
    assertTrue(testElement.$.secureDnsToggle.$.control.disabled);
    assertTrue(testElement.$.secureDnsRadioGroup.hidden);
    assertEquals(defaultDescription, testElement.$.secureDnsToggle.subLabel);
    assertTrue(!!testElement.$.secureDnsToggle.shadowRoot!.querySelector(
        'cr-policy-pref-indicator'));
    assertFalse(testElement.$.secureDnsToggle.shadowRoot!
                    .querySelector('cr-policy-pref-indicator')!.shadowRoot!
                    .querySelector('cr-tooltip-icon')!.hidden);
  });
});
