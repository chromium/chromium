// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrExpandButtonElement, SettingsSecureDnsV2Element, SettingsSecurityPageV2Element} from 'chrome://settings/lazy_load.js';
import {HttpsFirstModeSetting, JavascriptOptimizerSetting, SafeBrowsingSetting, SecuritySettingsBundleSetting} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import type {ControlledRadioButtonElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrivacyElementInteractions, Router, routes, resetRouterForTesting, SecureDnsMode, SecurityPageV2Interaction} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';

import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

// clang-format on

suite('Main', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageV2Element;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: true,
      enableBundledSecuritySettingsSecureDnsV2: false,
      enableBlockV8OptimizerOnUnfamiliarSites: true,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(setUpPage);

  function setUpPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    // Set initial pref values for test predictability.
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    page.setPrefValue(
        'generated.javascript_optimizer', JavascriptOptimizerSetting.ALLOWED);
    return flushTasks();
  }

  test('StandardBundleIsInitiallySelected', function() {
    assertEquals(
        SecuritySettingsBundleSetting.STANDARD,
        page.prefs.generated.security_settings_bundle.value);
  });

  test('EnhanceBundleSelected', async function() {
    // Standard bundle is initially selected.
    assertEquals(
        SecuritySettingsBundleSetting.STANDARD,
        page.prefs.generated.security_settings_bundle.value);

    // Click on Enhanced bundle.
    page.$.securitySettingsBundleEnhanced.click();
    await flushTasks();
    assertEquals(
        SecuritySettingsBundleSetting.ENHANCED,
        page.prefs.generated.security_settings_bundle.value);
  });

  test('SafeBrowsingRowClickExpandsRowAndShowsRadioGroup', async function() {
    assertFalse(isChildVisible(page, '#safeBrowsingRadioGroup'));

    const expandButton =
        page.$.safeBrowsingRow.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#expandButton');
    assertTrue(!!expandButton);

    // Click on the expand button, expands content and we can see the radio
    // group.
    expandButton.click();
    await flushTasks();
    assertTrue(isChildVisible(page, '#safeBrowsingRadioGroup'));

    // Verify that the correct User Action has been recorded.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'SafeBrowsing.Settings.SafeBrowsingRowExpanded',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');

    // Click on the expand button, collapses content and we can't see the radio
    // group.
    expandButton.click();
    await flushTasks();
    assertFalse(isChildVisible(page, '#safeBrowsingRadioGroup', true));
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));
  });

  test('SafeBrowsingToggledOff', async function() {
    // Verify no User Action has been recorded yet.
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));

    // Expand the Safe Browsing row.
    page.$.safeBrowsingRow.$.expandButton.click();
    await flushTasks();

    // Verify that the correct User Action has been recorded.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'SafeBrowsing.Settings.SafeBrowsingRowExpanded',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');

    // Toggle Safe Browsing off.
    const toggleButton =
        page.$.safeBrowsingRow.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#toggleButton');
    assertTrue(
        !!toggleButton, 'Safe Browsing toggle button element should exist.');
    assertTrue(
        isVisible(toggleButton),
        'The toggle button should be visible after expanding the row.');
    toggleButton.click();
    await flushTasks();

    // Verify that the correct User Action has been recorded.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingClicked',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('StandardRadioButtonSelected', async function() {
    // Set initial page state.
    page.$.securitySettingsBundleEnhanced.click();
    await flushTasks();

    // Verify no User Action has been recorded yet.
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));

    // Expand the Safe Browsing row.
    page.$.safeBrowsingRow.$.expandButton.click();
    await flushTasks();

    // Verify that the correct User Action has been recorded.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'SafeBrowsing.Settings.SafeBrowsingRowExpanded',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');

    // Click the Standard radio button.
    const radioGroup =
        page.shadowRoot!.querySelector<HTMLElement>('#safeBrowsingRadioGroup');
    assertTrue(!!radioGroup, 'Radio group element must be in the DOM.');
    assertTrue(
        isVisible(radioGroup),
        'The radio group should be visible after expanding the row.');

    const standardSafeBrowsingRadioButton =
        radioGroup.querySelector<ControlledRadioButtonElement>(
            'controlled-radio-button');
    assertTrue(
        !!standardSafeBrowsingRadioButton,
        'Standard Safe Browsing radio button element should exist.');
    assertTrue(
        isVisible(standardSafeBrowsingRadioButton),
        'The radio group should be visible after expanding the row.');

    standardSafeBrowsingRadioButton.click();
    await eventToPromise('change', radioGroup);
    await flushTasks();

    // Verify that the correct User Action has been recorded.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'SafeBrowsing.Settings.StandardProtectionClicked',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('EnhancedRadioButtonSelected', async function() {
    // Verify no User Action has been recorded yet.
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));

    // Expand the Safe Browsing row.
    page.$.safeBrowsingRow.$.expandButton.click();
    await flushTasks();

    // Verify that the correct User Action has been recorded.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'SafeBrowsing.Settings.SafeBrowsingRowExpanded',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');

    // Click the Enhanced radio button.
    const radioGroup =
        page.shadowRoot!.querySelector<HTMLElement>('#safeBrowsingRadioGroup');
    assertTrue(!!radioGroup, 'Radio group element must be in the DOM.');
    assertTrue(
        isVisible(radioGroup),
        'The radio group should be visible after expanding the row.');

    const enhancedSafeBrowsingRadioButton =
        radioGroup.querySelector<ControlledRadioButtonElement>(
            '#enhancedProtectionButton');
    assertTrue(
        !!enhancedSafeBrowsingRadioButton,
        'Enhanced Safe Browsing radio button element should exist.');
    assertTrue(
        isVisible(enhancedSafeBrowsingRadioButton),
        'The radio group should be visible after expanding the row.');

    enhancedSafeBrowsingRadioButton.click();
    await eventToPromise('change', radioGroup);
    await flushTasks();

    // Verify that the correct User Action has been recorded.
    assertEquals(1, testMetricsBrowserProxy.getCallCount('recordAction'));
    assertEquals(
        'SafeBrowsing.Settings.EnhancedProtectionClicked',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('ResetStandardBundleToDefaultsButtonVisibility', async function() {
    assertFalse(isChildVisible(page, '#resetStandardBundleToDefaultsButton'));

    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    await flushTasks();
    assertTrue(isChildVisible(page, '#resetStandardBundleToDefaultsButton'));
  });

  test('ResetEnhancedBundleToDefaultsButtonVisibility', async function() {
    page.$.securitySettingsBundleEnhanced.click();
    await flushTasks();
    assertFalse(isChildVisible(page, '#resetEnhancedBundleToDefaultsButton'));

    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    await flushTasks();
    assertTrue(isChildVisible(page, '#resetEnhancedBundleToDefaultsButton'));
  });

  test('ResetStandardToDefaultsClick', async function() {
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    await flushTasks();
    assertTrue(!!page.$.resetStandardBundleToDefaultsButton);
    assertTrue(isVisible(page.$.resetStandardBundleToDefaultsButton));

    page.$.resetStandardBundleToDefaultsButton.click();
    await flushTasks();
    assertEquals(
        SafeBrowsingSetting.STANDARD,
        page.getPref('generated.safe_browsing').value);
    assertFalse(isVisible(page.$.resetStandardBundleToDefaultsButton));
  });

  test('ResetEnhancedToDefaultsClick', async function() {
    page.$.securitySettingsBundleEnhanced.click();
    await flushTasks();
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    await flushTasks();
    assertTrue(!!page.$.resetEnhancedBundleToDefaultsButton);
    assertTrue(isVisible(page.$.resetEnhancedBundleToDefaultsButton));

    page.$.resetEnhancedBundleToDefaultsButton.click();
    await flushTasks();
    assertEquals(
        SafeBrowsingSetting.ENHANCED,
        page.getPref('generated.safe_browsing').value);
    assertFalse(isVisible(page.$.resetEnhancedBundleToDefaultsButton));
  });

  test('HttpsFirstModeRowRadioButtonsDisabledWhenOff', async function() {
    page.setPrefValue(
        'generated.https_first_mode_enabled', HttpsFirstModeSetting.DISABLED);
    await flushTasks();

    // The toggle is set to OFF because HTTPS First Mode is DISABLED.
    const toggle = page.$.httpsFirstModeToggle;
    assertFalse(toggle.checked, 'Toggle should be set to off');

    // The radio buttons are disabled because HTTPS First Mode is DISABLED.
    const balancedButton = page.$.httpsFirstModeEnabledBalanced;
    assertTrue(
        balancedButton.disabled, 'Balanced radio button should be disabled');
    const strictButton = page.$.httpsFirstModeEnabledStrict;
    assertTrue(strictButton.disabled, 'Strict radio button should be disabled');

    toggle.click();
    await flushTasks();

    // The toggle is set to ON, so the radio buttons are now enabled.
    assertFalse(
        balancedButton.disabled, 'Balanced radio button should be enabled');
    assertFalse(strictButton.disabled, 'Strict radio button should be enabled');
  });

  test('HttpsFirstModeDefaultBalancedWhenToggledOn', async function() {
    page.setPrefValue(
        'generated.https_first_mode_enabled', HttpsFirstModeSetting.DISABLED);
    await flushTasks();

    // The toggle is set to OFF because HTTPS First Mode is DISABLED.
    const toggle = page.$.httpsFirstModeToggle;
    assertFalse(toggle.checked, 'Toggle should be OFF');

    toggle.click();
    await flushTasks();
    assertTrue(toggle.checked, 'Toggle should be ON');

    // The pref should default to ENABLED_BALANCED.
    assertEquals(
        HttpsFirstModeSetting.ENABLED_BALANCED,
        page.getPref('generated.https_first_mode_enabled').value,
        'HTTPS First Mode should default to ENABLED_BALANCED when enabled');
  });

  test('SafeBrowsingWarningIconUpdatesCorrectly', async function() {
    const row = page.$.safeBrowsingRow;

    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    await flushTasks();
    assertFalse(
        row.iconVisible,
        'Icon should not be shown when enhanced safe browsing is enabled');

    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.DISABLED);
    await flushTasks();
    assertTrue(
        row.iconVisible, 'Icon should be shown when safe browsing is disabled');
    assertEquals('settings20:warning_outline', row.icon);

    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    await flushTasks();
    assertFalse(
        row.iconVisible,
        'Icon should not be shown when standard safe browsing is enabled');

    page.set('prefs.generated.safe_browsing', {
      ...page.get('prefs.generated.safe_browsing'),
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: SafeBrowsingSetting.DISABLED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    await flushTasks();
    assertFalse(
        row.iconVisible,
        'Icon should not be shown when safe browsing is disabled by policy');
  });

  test('PasswordsLeakDetectionClickTogglesSetting', async function() {
    page.setPrefValue('generated.password_leak_detection', true);

    page.$.passwordsLeakToggle.click();
    await flushTasks();
    assertFalse(page.getPref('generated.password_leak_detection').value);

    page.$.passwordsLeakToggle.click();
    await flushTasks();
    assertTrue(page.getPref('generated.password_leak_detection').value);
  });

  test('noValueChangePasswordLeakSwitchBundle', async () => {
    // Ensure password leak detection is initially disabled.
    page.setPrefValue('generated.password_leak_detection', false);

    // Click on Enhanced bundle.
    page.$.securitySettingsBundleEnhanced.click();
    await flushTasks();
    assertEquals(
        SecuritySettingsBundleSetting.ENHANCED,
        page.getPref('generated.security_settings_bundle').value,
        'Enhanced bundle should be selected');

    // Password leak detection value is unchanged.
    assertFalse(
        page.getPref('generated.password_leak_detection').value,
        `Password leak detection should not be changed by switching to
         Enhanced bundle`);

    // Click on Standard bundle.
    page.$.securitySettingsBundleStandard.click();
    await flushTasks();
    assertEquals(
        SecuritySettingsBundleSetting.STANDARD,
        page.getPref('generated.security_settings_bundle').value,
        'Standard bundle should be selected');

    // Password leak detection value is unchanged.
    assertFalse(
        page.getPref('generated.password_leak_detection').value,
        `Password leak detection should not be changed by switching to
         Standard bundle`);
  });

  test('ManageCertificatesClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#manageCertificatesLinkRow')!.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.MANAGE_CERTIFICATES, result);

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('certManagementV2URL'));
  });

  test('ManageSecurityKeysSubpageVisible', function() {
    assertTrue(isChildVisible(page, '#securityKeysSubpageTrigger'));
  });

  test('AdvancedProtectionProgramRowClick', async function() {
    page.shadowRoot!
        .querySelector<HTMLElement>(
            '#advancedProtectionProgramLinkRow')!.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('advancedProtectionURL'));
  });

  test('AdvancedProtectionProgramTextLinkClick', async function() {
    // Click Advanced Protection Program link.
    const linkRow = page.shadowRoot!.querySelector<HTMLElement>(
        '#advancedProtectionProgramLinkRow');
    assertTrue(!!linkRow, 'Link row should exist.');

    const linkElement = linkRow.querySelector('a');
    assertTrue(!!linkElement, 'Link element should exist.');

    const event = new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
    });
    linkElement.dispatchEvent(event);

    // Verify that the default action of link navigation was ignored.
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('advancedProtectionURL'));
    assertTrue(event.defaultPrevented);
  });

  test('SecureDnsV2HiddenWhenFlagDisabled', function() {
    // Secure DNS V2 row is hidden.
    assertFalse(
        loadTimeData.getBoolean('enableBundledSecuritySettingsSecureDnsV2'));
    assertFalse(isChildVisible(page, '#secureDnsV2Row'));

    // Old Secure DNS row is visible.
    assertTrue(isChildVisible(page, '#secureDnsRow'));
  });

  test('SecureDnsV2VisibleWhenFlagEnabled', async function() {
    loadTimeData.overrideValues({
      enableBundledSecuritySettingsSecureDnsV2: true,
    });
    resetRouterForTesting();

    await setUpPage();
    // Secure DNS V2 row is visible.
    assertTrue(
        loadTimeData.getBoolean('enableBundledSecuritySettingsSecureDnsV2'));
    assertTrue(isChildVisible(page, '#secureDnsV2Row'));

    // Old Secure DNS row is hidden.
    assertFalse(isChildVisible(page, '#secureDnsRow'));
  });

  test('JsGuardrailsAllowOnAllSites', async function() {
    assertEquals(
        SecuritySettingsBundleSetting.STANDARD,
        page.prefs.generated.security_settings_bundle.value);

    // Expand the row.
    page.$.javascriptGuardrailsRow.$.expandButton.click();
    await flushTasks();

    const toggleButton =
        page.$.javascriptGuardrailsRow.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#toggleButton');
    assertTrue(!!toggleButton);

    // Confirm that in the standard bundle the Javascript Guardrails is off.
    assertFalse(toggleButton.checked);
  });

  test('JsGuardrailsBlockOnUnfamiliarSites', async function() {
    // Expand the row.
    page.$.javascriptGuardrailsRow.$.expandButton.click();
    await flushTasks();

    const toggleButton =
        page.$.javascriptGuardrailsRow.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#toggleButton');
    assertTrue(!!toggleButton);

    // Click the toggle to turn on the JS optimizer setting.
    toggleButton.click();
    await flushTasks();

    // Confirm that in the standard bundle the Javascript Guardrails is on
    // and the block for unfamiliar sites radio button is checked.
    assertTrue(toggleButton.checked);
    assertTrue(page.$.blockForUnfamiliarSites.checked);
  });

  test('JsGuardrailsBlockOnAllSites', async function() {
    // Expand the row.
    page.$.javascriptGuardrailsRow.$.expandButton.click();
    await flushTasks();

    const blockForAllSitesButton = page.$.blockForAllSites;

    // Click the block for all sites button.
    blockForAllSitesButton.click()!;
    await flushTasks();

    const toggleButton =
        page.$.javascriptGuardrailsRow.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#toggleButton');
    assertTrue(!!toggleButton);

    // Clicking a radio button while the toggle is off should turn it on.
    assertTrue(toggleButton.checked);
    assertFalse(page.$.blockForUnfamiliarSites.checked);
    assertTrue(page.$.blockForAllSites.checked);
  });

  test(
      'JsGuardrailsManageSiteExceptionsNavigatesToContentSettings',
      async function() {
        // Expand the row.
        page.$.javascriptGuardrailsRow.$.expandButton.click();
        await flushTasks();

        // Click on manage site exceptions button.
        const manageSiteExceptionsButton = page.$.manageSiteExceptionsButton;
        manageSiteExceptionsButton.click()!;
        await flushTasks();

        assertEquals(
            routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER,
            Router.getInstance().getCurrentRoute());
      });
});

suite('SecurityKeysSubpageDisabled', function() {
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageV2Element;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: false,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    flush();
  });


  test('ManageSecurityKeysSubpageNotVisible', function() {
    assertFalse(isChildVisible(page, '#securityKeysSubpageTrigger'));
  });
});

suite('SecurityPageV2HappinessTrackingSurveys', function() {
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageV2Element;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableBundledSecuritySettingsSecureDnsV2: true,
    });
  });

  setup(async function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    // Set initial pref values for test predictability.
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    page.setPrefValue(
        'generated.https_first_mode_enabled',
        HttpsFirstModeSetting.ENABLED_BALANCED);

    // Navigate to the security route to trigger the setup logic in the
    // component, which sets up the event listeners and initial state.
    Router.getInstance().navigateTo(routes.SECURITY);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  // Checks that the `interactions` is equal to `expectedInteractions`, ignoring
  // order.
  function assertInteractionsEqual(
      expectedInteractions: SecurityPageV2Interaction[],
      interactions: SecurityPageV2Interaction[]) {
    interactions.sort((a, b) => a - b);
    expectedInteractions.sort((a, b) => a - b);

    assertDeepEquals(expectedInteractions, interactions);
  }

  test('SecurityPageV2_CallsHatsProxy', async function() {
    const t1 = 10000;
    testHatsBrowserProxy.setNow(t1);
    window.dispatchEvent(new Event('focus'));

    const t2 = 20000;
    testHatsBrowserProxy.setNow(t2);
    window.dispatchEvent(new Event('blur'));

    const t3 = 50000;
    testHatsBrowserProxy.setNow(t3);
    window.dispatchEvent(new Event('focus'));

    const t4 = 90000;
    testHatsBrowserProxy.setNow(t4);


    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    // Verify interactions.
    assertEquals(0, args[0].length);

    // Verify the safe browsing state on open.
    assertEquals(SafeBrowsingSetting.ENHANCED, args[1]);

    // Verify the time the user spent on the security page.
    const expectedTotalTimeInFocus = (t2 - t1) + (t4 - t3);
    assertEquals(expectedTotalTimeInFocus, args[2]);

    // Verify the security bundle state on open.
    assertEquals(SecuritySettingsBundleSetting.STANDARD, args[3]);
  });

  test('BeforeUnloadWithInteractions_CallsHatsProxy', async function() {
    const t1 = 10000;
    testHatsBrowserProxy.setNow(t1);
    window.dispatchEvent(new Event('focus'));

    const t2 = 20000;
    testHatsBrowserProxy.setNow(t2);

    // Interact with the Enhanced bundle radio button.
    page.$.securitySettingsBundleEnhanced.click();
    await flushTasks();

    // Expand the Safe Browsing row.
    const expandButton =
        page.$.safeBrowsingRow.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#expandButton')!;
    expandButton.click();
    await flushTasks();

    assertTrue(
        isVisible(page.$.safeBrowsingRadioGroup),
        'The radio group should be visible after expanding the row.');

    // Proceed to click the Standard button.
    const standardSafeBrowsingRadioButton =
        page.$.safeBrowsingRadioGroup
            .querySelector<ControlledRadioButtonElement>(
                'controlled-radio-button');
    assertTrue(
        !!standardSafeBrowsingRadioButton,
        'Standard Safe Browsing radio button element should exist.');
    standardSafeBrowsingRadioButton.click();
    await flushTasks();

    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    // Verify the interactions. Order doesn't matter, so check for
    // presence and length.
    const interactions = args[0] as SecurityPageV2Interaction[];
    const expectedInteractions = [
      SecurityPageV2Interaction.ENHANCED_BUNDLE_RADIO_BUTTON_CLICK,
      SecurityPageV2Interaction.SAFE_BROWSING_ROW_EXPANDED,
      SecurityPageV2Interaction.STANDARD_SAFE_BROWSING_RADIO_BUTTON_CLICK,
    ];
    assertInteractionsEqual(expectedInteractions, interactions);

    // Verify the safe browsing state on open.
    assertEquals(SafeBrowsingSetting.ENHANCED, args[1]);

    // Verify the time the user spent on the security page.
    const expectedTotalTimeInFocus = t2 - t1;
    assertEquals(expectedTotalTimeInFocus, args[2]);

    // Verify the security bundle state on open.
    assertEquals(SecuritySettingsBundleSetting.STANDARD, args[3]);
  });

  test('SafeBrowsingInteractions', async function() {
    // Expand the row first.
    const expandButton =
        page.$.safeBrowsingRow.shadowRoot!.querySelector<HTMLElement>(
            '#expandButton');
    assertTrue(!!expandButton);
    expandButton.click();
    await flushTasks();

    // Click radio buttons.
    const radioButtons = page.$.safeBrowsingRadioGroup
                             .querySelectorAll<ControlledRadioButtonElement>(
                                 'controlled-radio-button');
    // Index 0 is Standard, Index 1 is Enhanced.
    radioButtons[0]!.click();
    await flushTasks();
    radioButtons[1]!.click();
    await flushTasks();

    // Toggle Safe Browsing.
    const toggleButton = page.$.safeBrowsingRow.shadowRoot!.querySelector(
        'settings-toggle-button');
    assertTrue(!!toggleButton);
    toggleButton.click();
    await flushTasks();

    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    const interactions = args[0] as SecurityPageV2Interaction[];
    const expectedInteractions = [
      SecurityPageV2Interaction.SAFE_BROWSING_ROW_EXPANDED,
      SecurityPageV2Interaction.STANDARD_SAFE_BROWSING_RADIO_BUTTON_CLICK,
      SecurityPageV2Interaction.ENHANCED_SAFE_BROWSING_RADIO_BUTTON_CLICK,
      SecurityPageV2Interaction.SAFE_BROWSING_TOGGLE_CLICK,
    ];

    assertInteractionsEqual(expectedInteractions, interactions);
  });

  test('SecureDnsV2Interactions', async function() {
    assertTrue(
        loadTimeData.getBoolean('enableBundledSecuritySettingsSecureDnsV2'));
    const secureDnsV2Row =
        page.shadowRoot!.querySelector<SettingsSecureDnsV2Element>(
            '#secureDnsV2Row');
    assertTrue(!!secureDnsV2Row);

    // Expand the row first.
    const expandButton = secureDnsV2Row.$.featureRow.$.expandButton;
    expandButton.click();
    await flushTasks();

    // Click each of the radio buttons.
    secureDnsV2Row.$.customRadioButton.click();
    await flushTasks();
    secureDnsV2Row.$.fallbackRadioButton.click();
    await flushTasks();
    secureDnsV2Row.$.automaticRadioButton.click();
    await flushTasks();

    // Toggle Secure DNS.
    const toggleButton = secureDnsV2Row.$.featureRow.shadowRoot!.querySelector(
        'settings-toggle-button');
    assertTrue(!!toggleButton);
    toggleButton.click();
    await flushTasks();

    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    const interactions = args[0] as SecurityPageV2Interaction[];
    const expectedInteractions = [
      SecurityPageV2Interaction.SECURE_DNS_V2_ROW_EXPANDED,
      SecurityPageV2Interaction.SECURE_DNS_V2_FALLBACK_RADIO_BUTTON_CLICK,
      SecurityPageV2Interaction.SECURE_DNS_V2_CUSTOM_RADIO_BUTTON_CLICK,
      SecurityPageV2Interaction.SECURE_DNS_V2_AUTOMATIC_RADIO_BUTTON_CLICK,
      SecurityPageV2Interaction.SECURE_DNS_V2_TOGGLE_CLICK,
    ];

    assertInteractionsEqual(expectedInteractions, interactions);
  });

  test('HttpsFirstModeInteractions', async function() {
    // Start with HTTPS-First Mode DISABLED.
    page.setPrefValue(
        'generated.https_first_mode_enabled', HttpsFirstModeSetting.DISABLED);
    await flushTasks();

    // Set the HTTPS-First Mode toggle to ON (which also enables the radio
    // buttons).
    page.$.httpsFirstModeToggle.click();
    await flushTasks();

    // Click each of the HTTPS-First Mode radio buttons. Start with Strict since
    // switching the toggle ON selects Balanced by default.
    page.$.httpsFirstModeEnabledStrict.click();
    await flushTasks();
    page.$.httpsFirstModeEnabledBalanced.click();
    await flushTasks();

    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    const interactions = args[0] as SecurityPageV2Interaction[];
    const expectedInteractions = [
      SecurityPageV2Interaction.HTTPS_FIRST_MODE_TOGGLE_CLICK,
      SecurityPageV2Interaction.STRICT_HTTPS_FIRST_MODE_RADIO_BUTTON_CLICK,
      SecurityPageV2Interaction.BALANCED_HTTPS_FIRST_MODE_RADIO_BUTTON_CLICK,
    ];

    assertInteractionsEqual(expectedInteractions, interactions);
  });

  test('PasswordLeakDetectionInteraction', async function() {
    // Toggle Password Leak Detection.
    page.$.passwordsLeakToggle.click();
    await flushTasks();

    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    const interactions = args[0] as SecurityPageV2Interaction[];
    const expectedInteractions = [
      SecurityPageV2Interaction.PASSWORD_LEAK_DETECTION_TOGGLE_CLICK,
    ];

    assertInteractionsEqual(expectedInteractions, interactions);
  });
});

suite('SecurityPageV2HappinessTrackingSurveys_SecureDnsLegacy', function() {
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageV2Element;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableBundledSecuritySettingsSecureDnsV2: false,
    });
  });

  setup(async function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    // Set initial pref values for test predictability.
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    page.setPrefValue(
        'generated.https_first_mode_enabled',
        HttpsFirstModeSetting.ENABLED_BALANCED);

    Router.getInstance().navigateTo(routes.SECURITY);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('SecureDnsInteractions', async function() {
    assertFalse(
        loadTimeData.getBoolean('enableBundledSecuritySettingsSecureDnsV2'));
    const secureDnsRow = page.shadowRoot!.querySelector('settings-secure-dns');
    assertTrue(!!secureDnsRow);

    // Toggle Secure DNS.
    const toggleButton =
        secureDnsRow.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#secureDnsToggle');
    assertTrue(!!toggleButton);
    toggleButton.click();
    await flushTasks();

    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    const interactions = args[0] as SecurityPageV2Interaction[];

    assertDeepEquals(
        [SecurityPageV2Interaction.SECURE_DNS_TOGGLE_CLICK], interactions);
  });
});

suite('ManagedEnvironment', function() {
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageV2Element;

  setup(async function() {
    loadTimeData.overrideValues({
      enableBundledSecuritySettingsSecureDnsV2: true,
    });
    await setUpPage();
  });

  async function setUpPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    await flushTasks();

    // Ensure the bundles are always initially visible.
    assertTrue(isVisible(page.$.bundlesRadioGroup));
  }

  test('BundlesAreVisibleWhenNotEnforced', async function() {
    page.set('prefs.generated.safe_browsing', {
      ...page.get('prefs.generated.safe_browsing'),
      enforcement: undefined,
      controlledBy: undefined,
    });
    page.set('prefs.dns_over_https.mode', {
      ...page.get('prefs.dns_over_https.mode'),
      enforcement: undefined,
      controlledBy: undefined,
    });
    page.set('prefs.generated.javascript_optimizer', {
      ...page.get('prefs.generated.javascript_optimizer'),
      enforcement: undefined,
      controlledBy: undefined,
    });
    await flushTasks();

    assertTrue(isVisible(page.$.bundlesRadioGroup));
  });

  test('BundlesAreHiddenWhenSafeBrowsingIsEnforced', async function() {
    page.set('prefs.generated.safe_browsing', {
      ...page.get('prefs.generated.safe_browsing'),
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    });
    await flushTasks();

    assertFalse(isVisible(page.$.bundlesRadioGroup));
  });

  test('BundlesAreHiddenWhenSecureDnsEnforced', async function() {
    page.set('prefs.dns_over_https.mode', {
      type: chrome.settingsPrivate.PrefType.STRING,
      value: SecureDnsMode.SECURE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    });
    await flushTasks();

    assertFalse(isVisible(page.$.bundlesRadioGroup));
  });

  test('BundlesAreHiddenWhenJavascriptOptimizerEnforced', async function() {
    page.set('prefs.generated.javascript_optimizer', {
      ...page.get('prefs.generated.javascript_optimizer'),
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    });
    await flushTasks();

    assertFalse(isVisible(page.$.bundlesRadioGroup));
  });

  test('BundlesAreVisibleWhenSecureDnsEnforcedButNotBundled', async function() {
    loadTimeData.overrideValues({
      enableBundledSecuritySettingsSecureDnsV2: false,
    });
    await setUpPage();

    page.set('prefs.dns_over_https.mode', {
      type: chrome.settingsPrivate.PrefType.STRING,
      value: SecureDnsMode.SECURE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    });
    await flushTasks();

    assertTrue(isVisible(page.$.bundlesRadioGroup));
  });

  test('BundlesAreVisibleWhenSettingsOff', async function() {
    page.set('prefs.generated.safe_browsing', {
      ...page.get('prefs.generated.safe_browsing'),
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: SafeBrowsingSetting.DISABLED,
      enforcement: undefined,
      controlledBy: undefined,
    });
    await flushTasks();
    assertTrue(
        isVisible(page.$.bundlesRadioGroup),
        'Bundles should still be visible when Safe Browsing is off');

    page.set('prefs.dns_over_https.mode', {
      type: chrome.settingsPrivate.PrefType.STRING,
      value: SecureDnsMode.OFF,
      enforcement: undefined,
      controlledBy: undefined,
    });
    await flushTasks();
    assertTrue(
        isVisible(page.$.bundlesRadioGroup),
        'Bundles should still be visible when secure DNS is off');

    page.set('prefs.generated.javascript_optimizer', {
      ...page.get('prefs.generated.javascript_optimizer'),
      enforcement: undefined,
      controlledBy: undefined,
    });
    await flushTasks();
    assertTrue(
        isVisible(page.$.bundlesRadioGroup),
        'Bundles should still be visible when JavaScript guardrails is off');
  });

  test('SettingsAreVisibleWhenEnforced', async function() {
    page.set('prefs.generated.safe_browsing', {
      ...page.get('prefs.generated.safe_browsing'),
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    });
    page.set('prefs.dns_over_https.mode', {
      type: chrome.settingsPrivate.PrefType.STRING,
      value: SecureDnsMode.SECURE,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    });
    page.set('prefs.generated.javascript_optimizer', {
      ...page.get('prefs.generated.javascript_optimizer'),
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    });
    await flushTasks();

    assertFalse(isVisible(page.$.bundlesRadioGroup));

    assertTrue(isVisible(page.$.safeBrowsingRow));
    assertTrue(isChildVisible(page, '#secureDnsV2Row'));
    assertTrue(isVisible(page.$.javascriptGuardrailsRow));
  });
});

suite('SecureDnsBundling', function() {
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageV2Element;

  setup(async function() {
    loadTimeData.overrideValues({
      enableBundledSecuritySettingsSecureDnsV2: true,
    });
    await setUpPage();
  });

  async function setUpPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);

    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    page.setPrefValue(
        'generated.javascript_optimizer', JavascriptOptimizerSetting.ALLOWED);
    page.setPrefValue('dns_over_https.mode', SecureDnsMode.AUTOMATIC);
    page.setPrefValue('dns_over_https.templates', '');
    page.setPrefValue('dns_over_https.automatic_mode_fallback_to_doh', false);

    return flushTasks();
  }

  test('ResetStandardBundleToDefaultsButtonVisibility', async function() {
    assertFalse(
        isVisible(page.$.resetStandardBundleToDefaultsButton),
        'Initially hidden');

    // Change the mode to something other than AUTOMATIC.
    page.setPrefValue('dns_over_https.mode', SecureDnsMode.SECURE);
    await flushTasks();
    assertTrue(
        isVisible(page.$.resetStandardBundleToDefaultsButton),
        'Visible after mode change');

    // Reset the mode to AUTOMATIC.
    page.setPrefValue('dns_over_https.mode', SecureDnsMode.AUTOMATIC);
    await flushTasks();
    assertFalse(
        isVisible(page.$.resetStandardBundleToDefaultsButton),
        'Hidden after mode reset');

    // Change the fallback setting.
    page.setPrefValue('dns_over_https.automatic_mode_fallback_to_doh', true);
    await flushTasks();
    assertTrue(
        isVisible(page.$.resetStandardBundleToDefaultsButton),
        'Visible after fallback change');

    // Reset the fallback setting to false.
    page.setPrefValue('dns_over_https.automatic_mode_fallback_to_doh', false);
    await flushTasks();
    assertFalse(
        isVisible(page.$.resetStandardBundleToDefaultsButton),
        'Hidden after fallback reset');

    // Change the templates.
    page.setPrefValue('dns_over_https.templates', 'https://example.com');
    await flushTasks();
    assertTrue(
        isVisible(page.$.resetStandardBundleToDefaultsButton),
        'Visible after templates change');
  });

  test('ResetStandardToDefaultsClick', async function() {
    page.setPrefValue('dns_over_https.mode', SecureDnsMode.SECURE);
    page.setPrefValue('dns_over_https.templates', 'https://example.com');
    page.setPrefValue('dns_over_https.automatic_mode_fallback_to_doh', false);
    await flushTasks();
    assertTrue(isVisible(page.$.resetStandardBundleToDefaultsButton));

    page.$.resetStandardBundleToDefaultsButton.click();
    await flushTasks();
    assertEquals(
        SecureDnsMode.AUTOMATIC, page.getPref('dns_over_https.mode').value);
    assertEquals('', page.getPref('dns_over_https.templates').value);
    assertFalse(
        page.getPref('dns_over_https.automatic_mode_fallback_to_doh').value);
    assertFalse(isVisible(page.$.resetStandardBundleToDefaultsButton));
  });

  test('ResetEnhancedToDefaultsClick', async function() {
    page.$.securitySettingsBundleEnhanced.click();
    await flushTasks();
    page.setPrefValue('dns_over_https.mode', SecureDnsMode.OFF);
    await flushTasks();
    assertTrue(isVisible(page.$.resetEnhancedBundleToDefaultsButton));

    page.$.resetEnhancedBundleToDefaultsButton.click();
    await flushTasks();
    assertEquals(
        SecureDnsMode.AUTOMATIC, page.getPref('dns_over_https.mode').value);
    assertEquals('', page.getPref('dns_over_https.templates').value);
    assertTrue(
        page.getPref('dns_over_https.automatic_mode_fallback_to_doh').value);
    assertFalse(isVisible(page.$.resetEnhancedBundleToDefaultsButton));
  });

  test(
      'ResetButtonVisibilityNotAffectedWhenSecureDnsNotBundled',
      async function() {
        loadTimeData.overrideValues({
          enableBundledSecuritySettingsSecureDnsV2: false,
        });
        await setUpPage();

        assertFalse(isVisible(page.$.resetStandardBundleToDefaultsButton));

        // Effectively select FALLBACK mode.
        page.setPrefValue(
            'dns_over_https.automatic_mode_fallback_to_doh', true);
        await flushTasks();
        assertFalse(isVisible(page.$.resetStandardBundleToDefaultsButton));

        // Effectively select SECURE mode.
        page.setPrefValue('dns_over_https.mode', SecureDnsMode.SECURE);
        page.setPrefValue(
            'dns_over_https.automatic_mode_fallback_to_doh', false);
        page.setPrefValue('dns_over_https.templates', 'https://example.com');
        await flushTasks();
        assertFalse(isVisible(page.$.resetStandardBundleToDefaultsButton));
      });
});
