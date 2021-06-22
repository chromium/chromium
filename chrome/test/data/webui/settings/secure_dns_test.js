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
import {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
import {TestPrivacyPageBrowserProxy} from 'chrome://test/settings/test_privacy_page_browser_proxy.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

// clang-format on

suite('SettingsSecureDnsInput', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SecureDnsInputElement} */
  let testElement;

  /** @type {CrInputElement} */
  let crInput;

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
    PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('secure-dns-input');
    document.body.appendChild(testElement);
    flush();
    crInput = testElement.shadowRoot.querySelector('#input');
    assertFalse(crInput.invalid);
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
    assertFalse(crInput.invalid);
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
    assertTrue(crInput.invalid);
    assertTrue(testElement.isInvalid());
    assertEquals(probeFail, crInput.errorMessage);
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
    assertFalse(crInput.invalid);
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
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());
  });

  test('SecureDnsInputInvalid', async function() {
    // Enter an invalid input and trigger validation.
    testElement.value = invalidEntry;
    testBrowserProxy.setParsedEntry([]);
    testElement.validate();
    assertEquals(
        invalidEntry, await testBrowserProxy.whenCalled('parseCustomDnsEntry'));
    assertTrue(crInput.invalid);
    assertTrue(testElement.isInvalid());
    assertEquals(invalidFormat, crInput.errorMessage);

    // Trigger an input event and check that the error clears.
    crInput.fire('input');
    assertFalse(crInput.invalid);
    assertFalse(testElement.isInvalid());
    assertEquals(invalidEntry, testElement.value);
  });
});

suite('SettingsSecureDns', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsSecureDnsElement} */
  let testElement;

  /** @type {SettingsToggleButtonElement} */
  let secureDnsToggle;

  /** @type {CrRadioGroupElement} */
  let secureDnsRadioGroup;

  /** @type {!Array<!ResolverOption>} */
  const resolverList = [
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
    assertTrue(secureDnsToggle.hasAttribute('checked'));
    assertFalse(secureDnsToggle.shadowRoot.querySelector('cr-toggle').disabled);
    assertFalse(secureDnsRadioGroup.hidden);
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
    PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('settings-secure-dns');
    testElement.prefs = {
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(testElement);

    await testBrowserProxy.whenCalled('getSecureDnsSetting');
    await flushTasks();
    secureDnsToggle = testElement.shadowRoot.querySelector('#secureDnsToggle');
    secureDnsRadioGroup =
        testElement.shadowRoot.querySelector('#secureDnsRadioGroup');
    assertRadioButtonsShown();
    assertEquals(
        testBrowserProxy.secureDnsSetting.mode, secureDnsRadioGroup.selected);
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
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertFalse(secureDnsToggle.shadowRoot.querySelector('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(
        !!secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator'));
  });

  test('SecureDnsAutomatic', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertRadioButtonsShown();
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(
        !!secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator'));
    assertEquals(SecureDnsMode.AUTOMATIC, secureDnsRadioGroup.selected);
  });

  test('SecureDnsSecure', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertRadioButtonsShown();
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertFalse(
        !!secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator'));
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
  });

  test('SecureDnsManagedEnvironment', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      templates: [],
      managementMode: SecureDnsUiManagementMode.DISABLED_MANAGED,
    });
    flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.shadowRoot.querySelector('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(managedEnvironmentDescription, secureDnsToggle.subLabel);
    assertTrue(
        !!secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator'));
    assertTrue(
        secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator')
            .shadowRoot.querySelector('cr-tooltip-icon')
            .hidden);
  });

  test('SecureDnsParentalControl', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      templates: [],
      managementMode: SecureDnsUiManagementMode.DISABLED_PARENTAL_CONTROLS,
    });
    flush();
    assertFalse(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.shadowRoot.querySelector('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(parentalControlDescription, secureDnsToggle.subLabel);
    assertTrue(
        !!secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator'));
    assertTrue(
        secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator')
            .shadowRoot.querySelector('cr-tooltip-icon')
            .hidden);
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
    assertTrue(secureDnsToggle.hasAttribute('checked'));
    assertTrue(secureDnsToggle.shadowRoot.querySelector('cr-toggle').disabled);
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(defaultDescription, secureDnsToggle.subLabel);
    assertTrue(
        !!secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator'));
    assertFalse(
        secureDnsToggle.shadowRoot.querySelector('cr-policy-pref-indicator')
            .shadowRoot.querySelector('cr-tooltip-icon')
            .hidden);
  });
});
