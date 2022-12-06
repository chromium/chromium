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
import {SecureDnsInputElement, SettingsSecureDnsElement} from 'chrome://settings/lazy_load.js';
import {PrivacyPageBrowserProxyImpl, ResolverOption, SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    {name: 'Custom', value: '', policy: ''},
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

  suiteSetup(function() {
    loadTimeData.overrideValues({showSecureDnsSetting: true});
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
    // Start in automatic mode.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();

    // Click on the secure dns toggle to disable secure dns.
    testElement.$.secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);

    // Click on the secure dns toggle to go back to automatic mode.
    testElement.$.secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);

    assertFalse(focused(testElement.$.secureDnsInput));

    // Change the radio button to secure mode. The focus should be on the
    // custom text field and the mode pref should still be 'automatic'.
    testElement.$.secureDnsRadioGroup.querySelectorAll(
                                         'cr-radio-button')[1]!.click();
    assertTrue(testElement.$.secureDnsInput.matches(':focus-within'));
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);
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
    testElement.$.secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    assertFalse(focused(testElement.$.secureDnsInput));

    // Click on the secure dns toggle. Focus should be on the custom text field
    // and the mode pref should remain 'off' until the text field is blurred.
    testElement.$.secureDnsToggle.click();
    assertTrue(focused(testElement.$.secureDnsInput));
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
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
    const options =
        testElement.$.secureResolverSelect.querySelectorAll('option');
    assertEquals(4, options.length);

    for (let i = 0; i < options.length; i++) {
      assertEquals(resolverList[i]!.name, options[i]!.text);
      assertEquals(resolverList[i]!.value, options[i]!.value);
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
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
    assertEquals(0, testElement.$.secureResolverSelect.selectedIndex);
    assertEquals('none', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        'block', getComputedStyle(testElement.$.secureDnsInput).display);
    assertEquals('', testElement.$.secureDnsInput.value);
  });

  test('SecureDnsDropdownChangeInSecureMode', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: resolverList[1]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);

    const dropdownMenu = testElement.$.secureResolverSelect;
    const privacyPolicyLine = testElement.$.privacyPolicy;

    assertEquals(1, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[1]!.policy, privacyPolicyLine.querySelector('a')!.href);

    // Change to resolver2
    dropdownMenu.value = resolverList[2]!.value;
    dropdownMenu.dispatchEvent(new Event('change'));
    let args =
        await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals(resolverList[1]!.value, args[0]);
    assertEquals(resolverList[2]!.value, args[1]);
    assertEquals(2, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[2]!.policy, privacyPolicyLine.querySelector('a')!.href);
    assertEquals(
        resolverList[2]!.value,
        testElement.prefs.dns_over_https.templates.value);

    // Change to custom
    testBrowserProxy.reset();
    dropdownMenu.value = '';
    dropdownMenu.dispatchEvent(new Event('change'));
    args = await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals(resolverList[2]!.value, args[0]);
    assertEquals('', args[1]);
    assertEquals(0, dropdownMenu.selectedIndex);
    assertEquals('none', getComputedStyle(testElement.$.privacyPolicy).display);
    assertTrue(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
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
    dropdownMenu.value = resolverList[1]!.value;
    dropdownMenu.dispatchEvent(new Event('change'));
    args = await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals('', args[0]);
    assertEquals(resolverList[1]!.value, args[1]);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        resolverList[1]!.value,
        testElement.prefs.dns_over_https.templates.value);
    testBrowserProxy.reset();
    dropdownMenu.value = '';
    dropdownMenu.dispatchEvent(new Event('change'));
    args = await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals(resolverList[1]!.value, args[0]);
    assertEquals('', args[1]);
    assertEquals('some_input', testElement.$.secureDnsInput.value);
  });

  test('SecureDnsDropdownChangeInAutomaticMode', async function() {
    testElement.prefs.dns_over_https.templates.value = 'resolver1_template';
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      config: resolverList[1]!.value,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.$.secureDnsRadioGroup.selected);

    const dropdownMenu = testElement.$.secureResolverSelect;
    const privacyPolicyLine = testElement.$.privacyPolicy;

    // Select resolver3. This change should not be reflected in prefs.
    assertNotEquals(3, dropdownMenu.selectedIndex);
    dropdownMenu.value = resolverList[3]!.value;
    dropdownMenu.dispatchEvent(new Event('change'));
    const args =
        await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertNotEquals(resolverList[3]!.value, args[0]);
    assertEquals(resolverList[3]!.value, args[1]);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[3]!.policy, privacyPolicyLine.querySelector('a')!.href);
    assertEquals(
        'resolver1_template', testElement.prefs.dns_over_https.templates.value);

    // Click on the secure dns toggle to disable secure dns.
    testElement.$.secureDnsToggle.click();
    assertTrue(testElement.$.secureDnsRadioGroup.hidden);
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
    assertFalse(testElement.$.secureDnsRadioGroup.hidden);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[3]!.policy, privacyPolicyLine.querySelector('a')!.href);

    // Click on secure mode radio button.
    testElement.$.secureDnsRadioGroup.querySelectorAll(
                                         'cr-radio-button')[1]!.click();
    assertFalse(testElement.$.secureDnsRadioGroup.hidden);
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$.privacyPolicy).display);
    assertEquals(
        resolverList[3]!.policy, privacyPolicyLine.querySelector('a')!.href);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        'resolver3_template', testElement.prefs.dns_over_https.templates.value);
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
    assertEquals(
        'block', getComputedStyle(testElement.$.secureDnsInput).display);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(validEntry, testElement.$.secureDnsInput.value);
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);

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
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
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
    assertEquals(
        'block', getComputedStyle(testElement.$.secureDnsInput).display);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertTrue(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(invalidEntry, testElement.$.secureDnsInput.value);
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.$.secureDnsRadioGroup.selected);

    // Switching to automatic should remove focus from the input.
    assertFalse(focused(testElement.$.secureDnsInput));

    // Make the template valid, but don't change the radio button yet.
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
        SecureDnsMode.AUTOMATIC, testElement.$.secureDnsRadioGroup.selected);

    // Select the secure radio button and blur the input field.
    testElement.$.secureDnsRadioGroup.querySelectorAll(
                                         'cr-radio-button')[1]!.click();
    assertTrue(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);
    assertEquals('', testElement.prefs.dns_over_https.templates.value);
    testElement.$.secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('isValidConfig'),
      testBrowserProxy.whenCalled('probeConfig'),
    ]);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
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
    assertEquals(
        'block', getComputedStyle(testElement.$.secureDnsInput).display);
    assertFalse(testElement.$.secureDnsInput.matches(':focus-within'));
    assertFalse(testElement.$.secureDnsInput.$.input.invalid);
    assertEquals(managedDoubleEntry, testElement.$.secureDnsInput.value);
    assertEquals(
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
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
        SecureDnsMode.SECURE, testElement.$.secureDnsRadioGroup.selected);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        doubleValidEntry, testElement.prefs.dns_over_https.templates.value);
  });
});
