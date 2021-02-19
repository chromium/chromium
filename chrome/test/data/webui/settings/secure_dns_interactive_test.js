// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns and
 * secure-dns-input interactively.
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

/** @return {boolean} */
function focused(inputElement) {
  return inputElement.$$('#input').hasAttribute('focused_');
}

suite('SettingsSecureDnsInputInteractive', function() {
  /** @type {SecureDnsInputElement} */
  let testElement;

  setup(function() {
    assertTrue(document.hasFocus());
    PolymerTest.clearBody();
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
    {name: 'Custom', value: '', policy: ''},
    {
      name: 'resolver1',
      value: 'resolver1_template',
      policy: 'https://resolver1_policy.com/'
    },
    {
      name: 'resolver2',
      value: 'resolver2_template',
      policy: 'https://resolver2_policy.com/'
    },
    {
      name: 'resolver3',
      value: 'resolver3_template',
      policy: 'https://resolver3_policy.com/'
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
    secureDnsToggle = testElement.$$('#secureDnsToggle');
    secureDnsRadioGroup = testElement.$$('#secureDnsRadioGroup');
  });

  teardown(function() {
    testElement.remove();
  });

  test('SecureDnsModeChange', async function() {
    // Start in automatic mode.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);

    // Click on the secure dns toggle to go back to automatic mode.
    secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);

    const secureDnsInput = testElement.$$('#secureDnsInput');
    assertFalse(focused(secureDnsInput));

    // Change the radio button to secure mode. The focus should be on the
    // custom text field and the mode pref should still be 'automatic'.
    secureDnsRadioGroup.querySelectorAll('cr-radio-button')[1].click();
    assertTrue(secureDnsInput.matches(':focus-within'));
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);
    assertTrue(focused(secureDnsInput));

    // Enter a correctly formatted template in the custom text field and
    // click outside the text field. The mode pref should be updated to
    // 'secure'.
    secureDnsInput.value = validEntry;
    testBrowserProxy.setParsedEntry([validEntry]);
    testBrowserProxy.setProbeResults({[validEntry]: true});
    secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('parseCustomDnsEntry'),
      testBrowserProxy.whenCalled('probeCustomDnsTemplate')
    ]);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    assertFalse(focused(secureDnsInput));

    // Click on the secure dns toggle. Focus should be on the custom text field
    // and the mode pref should remain 'off' until the text field is blurred.
    secureDnsToggle.click();
    assertTrue(focused(secureDnsInput));
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertTrue(secureDnsInput.matches(':focus-within'));
    assertEquals(validEntry, secureDnsInput.value);
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('parseCustomDnsEntry'),
      testBrowserProxy.whenCalled('probeCustomDnsTemplate')
    ]);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
  });

  test('SecureDnsDropdown', function() {
    const options =
        testElement.$$('#secureResolverSelect').querySelectorAll('option');
    assertEquals(4, options.length);

    for (let i = 0; i < options.length; i++) {
      assertEquals(resolverList[i].name, options[i].text);
      assertEquals(resolverList[i].value, options[i].value);
    }
  });

  test('SecureDnsDropdownCustom', function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      templates: [''],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(0, testElement.$$('#secureResolverSelect').selectedIndex);
    assertEquals(
        'none', getComputedStyle(testElement.$$('#privacyPolicy')).display);
    assertEquals(
        'block', getComputedStyle(testElement.$$('#secureDnsInput')).display);
    assertEquals('', testElement.$$('#secureDnsInput').value);
  });

  test('SecureDnsDropdownChangeInSecureMode', async function() {
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      templates: [resolverList[1].value],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);

    const dropdownMenu = testElement.$$('#secureResolverSelect');
    const privacyPolicyLine = testElement.$$('#privacyPolicy');
    const secureDnsInput = testElement.$$('#secureDnsInput');

    assertEquals(1, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$$('#privacyPolicy')).display);
    assertEquals(
        resolverList[1].policy, privacyPolicyLine.querySelector('a').href);

    // Change to resolver2
    dropdownMenu.value = resolverList[2].value;
    dropdownMenu.dispatchEvent(new Event('change'));
    let args =
        await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals(resolverList[1].value, args[0]);
    assertEquals(resolverList[2].value, args[1]);
    assertEquals(2, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$$('#privacyPolicy')).display);
    assertEquals(
        resolverList[2].policy, privacyPolicyLine.querySelector('a').href);
    assertEquals(
        resolverList[2].value,
        testElement.prefs.dns_over_https.templates.value);

    // Change to custom
    testBrowserProxy.reset();
    dropdownMenu.value = '';
    dropdownMenu.dispatchEvent(new Event('change'));
    args = await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals(resolverList[2].value, args[0]);
    assertEquals('', args[1]);
    assertEquals(0, dropdownMenu.selectedIndex);
    assertEquals(
        'none', getComputedStyle(testElement.$$('#privacyPolicy')).display);
    assertTrue(secureDnsInput.matches(':focus-within'));
    assertFalse(secureDnsInput.isInvalid());
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        resolverList[2].value,
        testElement.prefs.dns_over_https.templates.value);

    // Input a custom template and make sure it is still there after
    // manipulating the dropdown.
    testBrowserProxy.reset();
    secureDnsInput.value = 'some_input';
    dropdownMenu.value = resolverList[1].value;
    dropdownMenu.dispatchEvent(new Event('change'));
    args = await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals('', args[0]);
    assertEquals(resolverList[1].value, args[1]);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        resolverList[1].value,
        testElement.prefs.dns_over_https.templates.value);
    testBrowserProxy.reset();
    dropdownMenu.value = '';
    dropdownMenu.dispatchEvent(new Event('change'));
    args = await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertEquals(resolverList[1].value, args[0]);
    assertEquals('', args[1]);
    assertEquals('some_input', secureDnsInput.value);
  });

  test('SecureDnsDropdownChangeInAutomaticMode', async function() {
    testElement.prefs.dns_over_https.templates.value = 'resolver1_template';
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [resolverList[1].value],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals(SecureDnsMode.AUTOMATIC, secureDnsRadioGroup.selected);

    const dropdownMenu = testElement.$$('#secureResolverSelect');
    const privacyPolicyLine = testElement.$$('#privacyPolicy');

    // Select resolver3. This change should not be reflected in prefs.
    assertNotEquals(3, dropdownMenu.selectedIndex);
    dropdownMenu.value = resolverList[3].value;
    dropdownMenu.dispatchEvent(new Event('change'));
    const args =
        await testBrowserProxy.whenCalled('recordUserDropdownInteraction');
    assertNotEquals(resolverList[3].value, args[0]);
    assertEquals(resolverList[3].value, args[1]);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$$('#privacyPolicy')).display);
    assertEquals(
        resolverList[3].policy, privacyPolicyLine.querySelector('a').href);
    assertEquals(
        'resolver1_template', testElement.prefs.dns_over_https.templates.value);

    // Click on the secure dns toggle to disable secure dns.
    secureDnsToggle.click();
    assertTrue(secureDnsRadioGroup.hidden);
    assertEquals(
        SecureDnsMode.OFF, testElement.prefs.dns_over_https.mode.value);
    assertEquals('', testElement.prefs.dns_over_https.templates.value);

    // Get another event enabling automatic mode.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [resolverList[1].value],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertFalse(secureDnsRadioGroup.hidden);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$$('#privacyPolicy')).display);
    assertEquals(
        resolverList[3].policy, privacyPolicyLine.querySelector('a').href);

    // Click on secure mode radio button.
    secureDnsRadioGroup.querySelectorAll('cr-radio-button')[1].click();
    assertFalse(secureDnsRadioGroup.hidden);
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(3, dropdownMenu.selectedIndex);
    assertEquals(
        'block', getComputedStyle(testElement.$$('#privacyPolicy')).display);
    assertEquals(
        resolverList[3].policy, privacyPolicyLine.querySelector('a').href);
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
      templates: [validEntry],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const secureDnsRadioGroup = testElement.$$('#secureDnsRadioGroup');
    const secureDnsInput = testElement.$$('#secureDnsInput');
    assertEquals('block', getComputedStyle(secureDnsInput).display);
    assertFalse(secureDnsInput.matches(':focus-within'));
    assertFalse(secureDnsInput.isInvalid());
    assertEquals(validEntry, secureDnsInput.value);
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);

    // Make the template invalid and check that the mode pref doesn't change.
    secureDnsInput.focus();
    assertTrue(focused(secureDnsInput));
    secureDnsInput.value = invalidEntry;
    testBrowserProxy.setParsedEntry([]);
    secureDnsInput.blur();
    await testBrowserProxy.whenCalled('parseCustomDnsEntry');
    assertFalse(secureDnsInput.matches(':focus-within'));
    assertTrue(secureDnsInput.isInvalid());
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(validEntry, testElement.prefs.dns_over_https.templates.value);

    // Receive a pref update and make sure the custom input field is not
    // cleared.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals('block', getComputedStyle(secureDnsInput).display);
    assertFalse(secureDnsInput.matches(':focus-within'));
    assertTrue(secureDnsInput.isInvalid());
    assertEquals(invalidEntry, secureDnsInput.value);
    assertEquals(SecureDnsMode.AUTOMATIC, secureDnsRadioGroup.selected);

    // Switching to automatic should remove focus from the input.
    assertFalse(focused(secureDnsInput));

    // Make the template valid, but don't change the radio button yet.
    secureDnsInput.focus();
    assertTrue(focused(secureDnsInput));
    const otherEntry = 'https://dns.ex.another/dns-query';
    secureDnsInput.value = `${validEntry} ${otherEntry}`;
    testBrowserProxy.setParsedEntry([validEntry, otherEntry]);
    testBrowserProxy.setProbeResults({[validEntry]: true});
    secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('parseCustomDnsEntry'),
      testBrowserProxy.whenCalled('probeCustomDnsTemplate')
    ]);
    assertFalse(secureDnsInput.matches(':focus-within'));
    assertFalse(secureDnsInput.isInvalid());
    assertEquals(SecureDnsMode.AUTOMATIC, secureDnsRadioGroup.selected);

    // Select the secure radio button and blur the input field.
    secureDnsRadioGroup.querySelectorAll('cr-radio-button')[1].click();
    assertTrue(secureDnsInput.matches(':focus-within'));
    assertFalse(secureDnsInput.isInvalid());
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(
        SecureDnsMode.AUTOMATIC, testElement.prefs.dns_over_https.mode.value);
    assertEquals('', testElement.prefs.dns_over_https.templates.value);
    secureDnsInput.blur();
    await Promise.all([
      testBrowserProxy.whenCalled('parseCustomDnsEntry'),
      testBrowserProxy.whenCalled('probeCustomDnsTemplate')
    ]);
    assertFalse(secureDnsInput.matches(':focus-within'));
    assertFalse(secureDnsInput.isInvalid());
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        `${validEntry} ${otherEntry}`,
        testElement.prefs.dns_over_https.templates.value);

    // Make sure the input field updates with a change in the underlying
    // templates pref in secure mode.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      templates: [
        'https://manage.ex/dns-query',
        'https://manage.ex.another/dns-query{?dns}'
      ],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    assertEquals('block', getComputedStyle(secureDnsInput).display);
    assertFalse(secureDnsInput.matches(':focus-within'));
    assertFalse(secureDnsInput.isInvalid());
    assertEquals(
        'https://manage.ex/dns-query https://manage.ex.another/dns-query{?dns}',
        secureDnsInput.value);
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
  });

  test('SecureDnsProbeFailure', async function() {
    // Start in secure mode with a valid template.
    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      templates: ['https://dns.example/dns-query'],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const secureDnsInput = testElement.$$('#secureDnsInput');

    // The input should not be focused automatically.
    assertFalse(focused(secureDnsInput));

    // Enter two valid templates that are both unreachable.
    secureDnsInput.focus();
    assertTrue(focused(secureDnsInput));
    const otherEntry = 'https://dns.ex.another/dns-query';
    secureDnsInput.value = `${validEntry} ${otherEntry}`;
    const parsed = [validEntry, otherEntry];
    testBrowserProxy.setParsedEntry(parsed);
    testBrowserProxy.setProbeResults({
      [validEntry]: false,
      [otherEntry]: false,
    });
    secureDnsInput.blur();
    assertEquals(
        secureDnsInput.value,
        await testBrowserProxy.whenCalled('parseCustomDnsEntry'));
    await flushTasks();
    assertEquals(2, testBrowserProxy.getCallCount('probeCustomDnsTemplate'));
    assertFalse(secureDnsInput.matches(':focus-within'));
    assertTrue(secureDnsInput.isInvalid());

    // Unreachable templates are accepted and committed anyway.
    assertEquals(SecureDnsMode.SECURE, secureDnsRadioGroup.selected);
    assertEquals(
        SecureDnsMode.SECURE, testElement.prefs.dns_over_https.mode.value);
    assertEquals(
        `${validEntry} ${otherEntry}`,
        testElement.prefs.dns_over_https.templates.value);
  });
});
