// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for settings-secure-dns and
 * secure-dns-input.
 */

// clang-format off
import 'chrome://os-settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SecureDnsInputElement, SettingsSecureDnsDialogElement, SettingsSecureDnsElement, SecureDnsResolverType} from 'chrome://os-settings/lazy_load.js';
import {PrivacyPageBrowserProxyImpl, ResolverOption, SecureDnsMode, LocalizedLinkElement, SecureDnsUiManagementMode, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertNull, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

import {clearBody} from '../utils.js';

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

    clearBody();
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

suite('SettingsSecureDns', () => {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsSecureDnsElement;
  let secureDnsToggle: SettingsToggleButtonElement;

  const resolverList: ResolverOption[] = [
    {
      name: 'Resolver 1',
      value: 'resolver',
      policy: 'https://resolver1_policy.com/',
    },
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

  suiteSetup(() => {
    loadTimeData.overrideValues({
      showSecureDnsSetting: true,
      secureDnsOsSettingsDescription: defaultDescription,
      secureDnsDisabledForManagedEnvironment: managedEnvironmentDescription,
      secureDnsDisabledForParentalControl: parentalControlDescription,
      isRevampWayfindingEnabled: false,
    });
  });

  setup(async () => {
    clearBody();
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    testBrowserProxy.setResolverList(resolverList);
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
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
      osMode: SecureDnsMode.OFF,
      osConfig: '',
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
      osMode: SecureDnsMode.AUTOMATIC,
      osConfig: '',
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
      osMode: SecureDnsMode.SECURE,
      osConfig: resolverList[0]!.value,
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
      osMode: SecureDnsMode.OFF,
      osConfig: '',
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
      osMode: SecureDnsMode.OFF,
      osConfig: '',
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
      osMode: SecureDnsMode.AUTOMATIC,
      osConfig: '',
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
      osMode: SecureDnsMode.SECURE,
      osConfig: effectiveConfig,
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

  test('SecureDnsManagedWithDohDomainConfig', function() {
    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;

    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.OFF,
      config: '',
      osMode: SecureDnsMode.SECURE,
      osConfig: 'https://example/dns-query',
      dohDomainConfigSet: true,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const expectedDescription =
        loadTimeData.getString('secureDnsWithDomainConfigDescription');
    assertEquals(expectedDescription, secureDnsToggle.subLabel);
  });

  test('SecureDnsManagedWithIdentifiersAndDomainConfig', function() {
    testElement.prefs.dns_over_https.mode.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    testElement.prefs.dns_over_https.mode.controlledBy =
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;

    const effectiveConfig = 'https://example/dns-query';
    const displayConfig = 'https://example-for-display/dns-query';

    webUIListenerCallback('secure-dns-setting-changed', {
      mode: SecureDnsMode.SECURE,
      config: effectiveConfig,
      osMode: SecureDnsMode.SECURE,
      osConfig: effectiveConfig,
      dohWithIdentifiersActive: true,
      configForDisplay: displayConfig,
      dohDomainConfigSet: true,
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    });
    flush();
    const expectedDescription = loadTimeData.substituteString(
        loadTimeData.getString(
            'secureDnsWithIdentifiersAndDomainConfigDescription'),
        displayConfig);
    assertEquals(expectedDescription, secureDnsToggle.subLabel);
  });

  suite('dropdown description', () => {
    let networkDefaultDescription: HTMLElement|null;
    let privacyPolicyDescription: LocalizedLinkElement|null;

    function assertDropdownDescriptionVisibility(
        networkDefault: boolean, privacyPolicy: boolean): void {
      networkDefaultDescription =
          testElement.shadowRoot!.querySelector('#networkDefaultDescription');
      privacyPolicyDescription =
          testElement.shadowRoot!.querySelector('#privacyPolicy');

      assertEquals(networkDefault, isVisible(networkDefaultDescription));
      assertEquals(privacyPolicy, isVisible(privacyPolicyDescription));
    }

    test(
        'When Secure is selected, Privacy Policy description appears.',
        async () => {
          webUIListenerCallback('secure-dns-setting-changed', {
            mode: SecureDnsMode.SECURE,
            config: resolverList[0]!.value,
            osMode: SecureDnsMode.SECURE,
            osConfig: resolverList[0]!.value,
            managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
          });
          await flushTasks();

          assertResolverSelectShown();
          assertDropdownDescriptionVisibility(
              /*networkDefault=*/ false, /*privacyPolicy=*/ true);
        });

    test(
        'When Automatic is selected, description for Network Default appears.',
        async () => {
          webUIListenerCallback('secure-dns-setting-changed', {
            mode: SecureDnsMode.AUTOMATIC,
            config: '',
            osMode: SecureDnsMode.AUTOMATIC,
            osConfig: '',
            managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
          });
          await flushTasks();

          assertResolverSelectShown();
          assertDropdownDescriptionVisibility(
              /*networkDefault=*/ true, /*privacyPolicy=*/ false);
        });

    test(
        'When switched from Automatic to OFF, no description appears.',
        async () => {
          webUIListenerCallback('secure-dns-setting-changed', {
            mode: SecureDnsMode.OFF,
            config: '',
            osMode: SecureDnsMode.OFF,
            osConfig: '',
            managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
          });
          await flushTasks();

          assertDropdownDescriptionVisibility(
              /*networkDefault=*/ false, /*privacyPolicy=*/ false);
        });

    test('When Custom is selected, no description appears.', async () => {
      webUIListenerCallback('secure-dns-setting-changed', {
        mode: SecureDnsMode.SECURE,
        config: '',
        osMode: SecureDnsMode.SECURE,
        osConfig: '',
        managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
      });
      await flushTasks();

      assertDropdownDescriptionVisibility(
          /*networkDefault=*/ false, /*privacyPolicy=*/ false);
    });

    test(
        'When switched from Secure to Custom, no description appears.',
        async () => {
          webUIListenerCallback('secure-dns-setting-changed', {
            mode: SecureDnsMode.SECURE,
            config: resolverList[0]!.value,
            osMode: SecureDnsMode.SECURE,
            osConfig: resolverList[0]!.value,
            managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
          });
          await flushTasks();

          webUIListenerCallback('secure-dns-setting-changed', {
            mode: SecureDnsMode.SECURE,
            config: '',
            osMode: SecureDnsMode.SECURE,
            osConfig: '',
            managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
          });
          await flushTasks();

          assertResolverSelectShown();
          assertDropdownDescriptionVisibility(
              /*networkDefault=*/ false, /*privacyPolicy=*/ false);
        });
  });
});

suite('SecureDnsDialog', () => {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsSecureDnsElement;
  let secureDnsToggle: SettingsToggleButtonElement;
  let secureDnsToggleDialog: SettingsSecureDnsDialogElement;
  const isDeprecateDnsDialogEnabled =
      loadTimeData.getBoolean('isDeprecateDnsDialogEnabled');
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');

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
    assertTrue(!!secureDnsToggleDialog.$.dialog);
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
    });
  });

  setup(async function() {
    clearBody();

    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    testElement = document.createElement('settings-secure-dns');
    testElement.prefs = {
      'dns_over_https': {
        'mode': {'value': SecureDnsMode.AUTOMATIC},
        'templates': {'value': ''},
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

  if (isDeprecateDnsDialogEnabled || !isRevampWayfindingEnabled) {
    test(
        'No warning dialog appears when secure DNS is toggled off',
        async () => {
          // Initiate a toggle change from on to off.
          secureDnsToggle.click();
          await flushTasks();

          secureDnsToggleDialog =
              testElement.shadowRoot!.querySelector('#warningDialog')!;
          assertNull(secureDnsToggleDialog);
          assertFalse(secureDnsToggle.checked);
          assertTrue(getResolverOptions().hidden);
        });
  } else {
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
        osMode: SecureDnsMode.OFF,
        osConfig: '',
        managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
      });
      flush();

      assertFalse(secureDnsToggle.checked);
      assertTrue(getResolverOptions().hidden);
    });

    test('SecureDnsDialogSecureOffToOn', () => {
      // If the user selects Custom Secure mode with an invalid input, we will
      // not register that the user wants to use secure mode (see the comment on
      // secure_dns_dialog.ts), however, when toggled off and on, we will still
      // show that the user had selected Custom before.

      // Select Secure in the menu button with no input and config.
      webUIListenerCallback('secure-dns-setting-changed', {
        mode: SecureDnsMode.SECURE,
        config: '',
        osMode: SecureDnsMode.SECURE,
        osConfig: '',
        managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
      });
      testElement.prefs = {
        'dns_over_https': {
          'mode': {'value': SecureDnsMode.SECURE},
          'templates': {'value': ''},
        },
      };

      // Simulate that the toggle is off.
      webUIListenerCallback('secure-dns-setting-changed', {
        mode: SecureDnsMode.OFF,
        config: '',
        osMode: SecureDnsMode.OFF,
        osConfig: '',
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
  }
});
