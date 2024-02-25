// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns and
 * secure-dns-input interactively.
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

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

function focused(inputElement: HTMLElement): boolean {
  return inputElement.shadowRoot!.querySelector('#input')!.hasAttribute(
      'focused_');
}

suite('SettingsSecureDnsInputInteractive', function() {
  let testElement: SecureDnsInputElement;

  setup(function() {
    assertTrue(document.hasFocus());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('secure-dns-input');
    document.body.appendChild(testElement);
    flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsInputFocus', function() {
    assertFalse(focused(testElement));
    testElement.focus();
    assertTrue(focused(testElement));
    testElement.blur();
    assertFalse(focused(testElement));
  });
});

suite('SettingsSecureDnsInteractive', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsSecureDnsElement;

  const resolverList: ResolverOption[] = [
    {
      name: 'resolver1',
      value: 'resolver1_template',
      policy: 'https://resolver1_policy.com/',
    },
    {
      name: 'resolver2',
      value: 'resolver2_template',
      policy: 'https://resolver2_policy.com/',
    },
    {
      name: 'resolver3',
      value: 'resolver3_template',
      policy: 'https://resolver3_policy.com/',
    },
  ];

  const invalidEntry = 'invalid_template';
  const validEntry = 'https://example.doh.server/dns-query';

  function getSecureDnsToggle(): SettingsToggleButtonElement {
    const secureDnsToggle =
        testElement.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#secureDnsToggle');
    assertTrue(!!secureDnsToggle);
    return secureDnsToggle;
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
      isRevampWayfindingEnabled: false,
    });
  });

  setup(async function() {
    assertTrue(document.hasFocus());
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
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsModeChange', async function() {
    const secureDnsToggle = getSecureDnsToggle();

    // Start in automatic mode.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    assertTrue(getResolverOptions().hidden);

    // Click on the secure dns toggle to go back to automatic mode.
    secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);

    assertFalse(getResolverOptions().hidden);
    assertFalse(focused(testElement.$.secureDnsInput));
    assertTrue(testElement.$.secureDnsInputContainer.hidden);

    // Change the resolver to the custom entry. The focus should be on the
    // custom text field and the mode pref should still be 'automatic'.
    testElement.$.resolverSelect.value = SecureDnsResolverType.CUSTOM;
    testElement.$.resolverSelect.dispatchEvent(new Event('change'));
    assertTrue(testElement.$.secureDnsInput.matches(':focus-within'));
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);
    assertFalse(testElement.$.secureDnsInputContainer.hidden);
    assertTrue(focused(testElement.$.secureDnsInput));

    // Enter a correctly formatted template in the custom text field and
    // click outside the text field. The mode pref should be updated to
    // 'secure'.
    testElement.$.secureDnsInput.value = validEntry;
    testBrowserProxy.setIsValidConfigResult(validEntry, true);
    testBrowserProxy.setProbeConfigResult(validEntry, true);
    testElement.$.secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('isValidConfig'),
      testBrowserProxy.whenCalled('probeConfig'),
    ]);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    assertFalse(focused(testElement.$.secureDnsInput));
    assertTrue(getResolverOptions().hidden);

    // Click on the secure dns toggle. Focus should be on the custom text field
    // and the mode pref should remain 'off' until the text field is blurred.
    secureDnsToggle.click();
    assertFalse(getResolverOptions().hidden);
    assertTrue(focused(testElement.$.secureDnsInput));
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);
    assertTrue(testElement.$.secureDnsInput.matches(':focus-within'));
    assertEquals(validEntry, testElement.$.secureDnsInput.value);
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    testElement.$.secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('isValidConfig'),
      testBrowserProxy.whenCalled('probeConfig'),
    ]);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
  });

  test('SecureDnsDropdown', function() {
    const options = testElement.$.resolverSelect.querySelectorAll('option');
    assertEquals(5, options.length);

    assertEquals(SecureDnsResolverType.AUTOMATIC, options[0]!.value);
    assertEquals(SecureDnsResolverType.CUSTOM, options[1]!.value);

    for (let i = 2; i < options.length; i++) {
      assertEquals(resolverList[i - 2]!.name, options[i]!.text);
      assertEquals(`${i - 2}`, options[i]!.value);
      assertEquals(
          SecureDnsResolverType.BUILT_IN, options[i]!.dataset['resolverType']);
    }
  });

  test('SecureDnsDropdownCustom', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);
    assertEquals('none', getComputedStyle(testElement.$.privacyPolicy).display);
    assertFalse(testElement.$.secureDnsInputContainer.hidden);
    assertEquals('', testElement.$.secureDnsInput.value);
  });

  test('SecureDnsDropdownChangeInSecureMode', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: resolverList[1]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();

    const dropdownMenu = testElement.$.resolverSelect;
    const privacyPolicyLine = testElement.$.privacyPolicy;

    // Currently selected resolver2.
    assertEquals('1', dropdownMenu.value);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[1]!.policy, privacyPolicyLine.querySelector('a')!.href);

    // Change to resolver3.
    dropdownMenu.value = '2';
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals('2', dropdownMenu.value);
    assertEquals(4, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[2]!.policy, privacyPolicyLine.querySelector('a')!.href);
    assertEquals(
        resolverList[2]!.value,
        testElement.prefs.dns_over_https.templates.value);

    // Change to custom.
    testBrowserProxy.reset();
    dropdownMenu.value = SecureDnsResolverType.CUSTOM;
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals(SecureDnsResolverType.CUSTOM, dropdownMenu.value);
    assertEquals(1, dropdownMenu.selectedIndex);
    assertEquals('none', getComputedStyle(testElement.$.privacyPolicy).display);
    assertTrue(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        resolverList[2]!.value,
        testElement.prefs.dns_over_https.templates.value);

    // Input a custom template and make sure it is still there after
    // manipulating the dropdown.
    testBrowserProxy.reset();
    testBrowserProxy.setIsValidConfigResult('some_input', false);
    testElement.$.secureDnsInput.value = 'some_input';
    dropdownMenu.value = '1';
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals('1', dropdownMenu.value);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        resolverList[1]!.value,
        testElement.prefs.dns_over_https.templates.value);
    testBrowserProxy.reset();
    dropdownMenu.value = SecureDnsResolverType.CUSTOM;
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals(SecureDnsResolverType.CUSTOM, dropdownMenu.value);
    assertEquals('some_input', testElement.$.secureDnsInput.value);
  });

  test('SecureDnsDropdownChangeInAutomaticMode', async function() {
    const secureDnsToggle = getSecureDnsToggle();

    testElement.prefs.dns_over_https.templates.value = 'resolver1_template';
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: resolverList[1]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();

    const dropdownMenu = testElement.$.resolverSelect;
    const privacyPolicyLine = testElement.$.privacyPolicy;

    assertEquals(SecureDnsResolverType.AUTOMATIC, dropdownMenu.value);

    // Select resolver3.
    dropdownMenu.value = '2';
    dropdownMenu.dispatchEvent(new Event('change'));
    assertEquals('2', dropdownMenu.value);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[2]!.policy, privacyPolicyLine.querySelector('a')!.href);
    assertEquals(
        'resolver3_template', testElement.prefs.dns_over_https.templates.value);

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertTrue(getResolverOptions().hidden);
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    assertEquals('', testElement.prefs.dns_over_https.templates.value);

    // Get another event enabling automatic mode.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: resolverList[1]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertFalse(getResolverOptions().hidden);
    assertEquals(SecureDnsResolverType.AUTOMATIC, dropdownMenu.value);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[1]!.policy, privacyPolicyLine.querySelector('a')!.href);

    // Switch to resolver 2.
    dropdownMenu.value = '1';
    dropdownMenu.dispatchEvent(new Event('change'));
    assertFalse(getResolverOptions().hidden);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[1]!.policy, privacyPolicyLine.querySelector('a')!.href);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        'resolver2_template', testElement.prefs.dns_over_https.templates.value);
  });

  test('SecureDnsInputChange', async function() {
    // Start in secure mode with a custom valid template
    testElement.prefs = {
      dns_over_https:
          {mode: {value: SecureDnsMode.SECURE}, templates: {value: validEntry}},
    };
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: validEntry,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertFalse(testElement.$.secureDnsInputContainer.hidden);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(validEntry, testElement.$.secureDnsInput.value);
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);

    // Make the template invalid and check that the mode pref doesn't change.
    testElement.$.secureDnsInput.focus();
    assertTrue(focused(testElement.$.secureDnsInput));
    testElement.$.secureDnsInput.value = invalidEntry;
    testBrowserProxy.setIsValidConfigResult(invalidEntry, false);
    testElement.$.secureDnsInput.blur();
    await testBrowserProxy.whenCalled('isValidConfig');
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertTrue(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(validEntry, testElement.prefs.dns_over_https.templates.value);

    // Receive a pref update and make sure the custom input field is not
    // cleared.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertTrue(testElement.$.secureDnsInputContainer.hidden);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertTrue(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(invalidEntry, testElement.$.secureDnsInput.value);
    assertEquals(SecureDnsMode.AUTOMATIC, testElement.$.resolverSelect.value);

    // Switching to automatic should remove focus from the input.
    assertFalse(focused(testElement.$.secureDnsInput));

    // Change back to custom and enter a double entry.
    testElement.$.resolverSelect.value = SecureDnsResolverType.CUSTOM;
    testElement.$.resolverSelect.dispatchEvent(new Event('change'));
    assertTrue(testElement.$.secureDnsInput.matches(':focus-within'));
    assertTrue(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(validEntry, testElement.prefs.dns_over_https.templates.value);
    testElement.$.secureDnsInput.focus();
    assertTrue(focused(testElement.$.secureDnsInput));
    const doubleValidEntry = `${validEntry} https://dns.ex.another/dns-query`;
    testElement.$.secureDnsInput.value = doubleValidEntry;
    testBrowserProxy.setIsValidConfigResult(doubleValidEntry, true);
    testBrowserProxy.setProbeConfigResult(doubleValidEntry, true);
    testElement.$.secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('isValidConfig'),
      testBrowserProxy.whenCalled('probeConfig'),
    ]);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        doubleValidEntry, testElement.prefs.dns_over_https.templates.value);

    // Make sure the input field updates with a change in the underlying
    // config pref in secure mode.
    const managedDoubleEntry =
        'https://manage.ex/dns-query https://manage.ex.another/dns-query{?dns}';
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: managedDoubleEntry,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertFalse(testElement.$.secureDnsInputContainer.hidden);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(managedDoubleEntry, testElement.$.secureDnsInput.value);
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);
  });

  test('SecureDnsProbeFailure', async function() {
    // Start in secure mode with a valid template.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: 'https://dns.example/dns-query',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();

    // The input should not be focused automatically.
    assertFalse(focused(testElement.$.secureDnsInput));
    assertFalse(testElement.$.secureDnsInputContainer.hidden);

    // Enter two valid templates that are both unreachable.
    testElement.$.secureDnsInput.focus();
    assertTrue(focused(testElement.$.secureDnsInput));
    const doubleValidEntry = `${validEntry} https://dns.ex.another/dns-query`;
    testElement.$.secureDnsInput.value = doubleValidEntry;
    testBrowserProxy.setIsValidConfigResult(doubleValidEntry, true);
    testBrowserProxy.setProbeConfigResult(doubleValidEntry, false);
    testElement.$.secureDnsInput.blur();
    assertEquals(
        testElement.$.secureDnsInput.value,
        await testBrowserProxy.whenCalled('isValidConfig'));
    await flushTasks();
    assertEquals(1, testBrowserProxy.getCallCount('probeConfig'));
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertTrue(testElement.$.secureDnsInput.$.input.invalid);

    // Unreachable templates are accepted and committed anyway.
    assertEquals(
        SecureDnsResolverType.CUSTOM, testElement.$.resolverSelect.value);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        doubleValidEntry, testElement.prefs.dns_over_https.templates.value);
  });
});
