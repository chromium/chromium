// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrExpandButtonElement, SettingsSecurityPageV2Element} from 'chrome://settings/lazy_load.js';
import {HttpsFirstModeSetting, SafeBrowsingSetting, SecuritySettingsBundleSetting} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import type {ControlledRadioButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, HatsBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrivacyElementInteractions, Router, routes, SecurityPageV2Interaction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

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
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    flush();
  });

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

  test('SafeBrowsingRowClickExpandsRowAndShowsSafeBrowsingSettings', async function() {
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

    // Click on the expand button, collapses content and we can't see the radio
    // group.
    expandButton.click();
    await flushTasks();
    assertFalse(isChildVisible(page, '#safeBrowsingRadioGroup', true));
  });

  test('ResetStandardBundleToDefaultsButtonVisibility', async function() {
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    await flushTasks();
    assertFalse(isChildVisible(page, '#resetStandardBundleToDefaultsButton'));

    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    await flushTasks();
    assertTrue(isChildVisible(page, '#resetStandardBundleToDefaultsButton'));
  });

  test('ResetEnhancedBundleToDefaultsButtonVisibility', async function() {
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.ENHANCED);
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    await flushTasks();
    assertFalse(isChildVisible(page, '#resetEnhancedBundleToDefaultsButton'));

    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    await flushTasks();
    assertTrue(isChildVisible(page, '#resetEnhancedBundleToDefaultsButton'));
  });

  test('ResetStandardToDefaultsClick', async function() {
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);
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
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.ENHANCED);
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

    // Ensure bundle is initially set to Standard.
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);

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
    page.shadowRoot!
        .querySelector<HTMLElement>(
            '#advancedProtectionProgramLinkRow')!.querySelector('a')!.click();

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('advancedProtectionURL'));
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
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    // Set initial pref values for test predictability.
    page.setPrefValue(
        'generated.security_settings_bundle',
        SecuritySettingsBundleSetting.STANDARD);
    page.setPrefValue('generated.safe_browsing', SafeBrowsingSetting.ENHANCED);

    // Navigate to the security route to trigger the setup logic in the
    // component, which sets up the event listeners and initial state.
    Router.getInstance().navigateTo(routes.SECURITY);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

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

    const radioGroup =
        page.shadowRoot!.querySelector<HTMLElement>('#safeBrowsingRadioGroup');
    assertTrue(!!radioGroup, 'Radio group element must be in the DOM.');
    assertTrue(
        isVisible(radioGroup),
        'The radio group should be visible after expanding the row.');

    // Proceed to click the Standard button.
    const standardSafeBrowsingRadioButton =
        radioGroup.querySelector<ControlledRadioButtonElement>(
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
    assertEquals(3, interactions.length);
    assertTrue(interactions.includes(
        SecurityPageV2Interaction.ENHANCED_BUNDLE_RADIO_BUTTON_CLICK));
    assertTrue(interactions.includes(
        SecurityPageV2Interaction.SAFE_BROWSING_ROW_EXPANDED));
    assertTrue(interactions.includes(
        SecurityPageV2Interaction.STANDARD_SAFE_BROWSING_RADIO_BUTTON_CLICK));

    // Verify the safe browsing state on open.
    assertEquals(SafeBrowsingSetting.ENHANCED, args[1]);

    // Verify the time the user spent on the security page.
    const expectedTotalTimeInFocus = t2 - t1;
    assertEquals(expectedTotalTimeInFocus, args[2]);

    // Verify the security bundle state on open.
    assertEquals(SecuritySettingsBundleSetting.STANDARD, args[3]);
  });
});
