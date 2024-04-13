// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSecurityPageElement} from 'chrome://settings/lazy_load.js';
import {HttpsFirstModeSetting, SafeBrowsingSetting} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {HatsBrowserProxyImpl, CrSettingsPrefs, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrivacyElementInteractions, PrivacyPageBrowserProxyImpl, Router, routes, SafeBrowsingInteractions, SecureDnsMode, SecurityPageInteraction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

function pagePrefs() {
  return {
    profile: {password_manager_leak_detection: {value: false}},
    safebrowsing: {
      scout_reporting_enabled: {value: true},
      esb_opt_in_with_friendlier_settings: {value: false},
    },
    generated: {
      safe_browsing: {
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: SafeBrowsingSetting.STANDARD,
      },
      password_leak_detection: {value: false},
      https_first_mode_enabled: {
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: HttpsFirstModeSetting.DISABLED,
      },
    },
    dns_over_https:
        {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    https_only_mode_enabled: {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: HttpsFirstModeSetting.DISABLED,
    },
  };
}

suite('Main', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testPrivacyBrowserProxy: TestPrivacyPageBrowserProxy;
  let page: SettingsSecurityPageElement;
  let openWindowProxy: TestOpenWindowProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: true,
      enableHttpsFirstModeNewSettings: true,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testPrivacyBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-security-page');
    page.prefs = pagePrefs();
    document.body.appendChild(page);
    page.$.safeBrowsingEnhanced.updateCollapsed();
    page.$.safeBrowsingStandard.updateCollapsed();
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('ChromeRootStorePage', async function() {
    const row =
        page.shadowRoot!.querySelector<HTMLElement>('#chromeCertificates');
    // <if expr="is_chromeos">
    assertTrue(!!row, 'Chrome Root Store Help Center link not found');
    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('chromeRootStoreHelpCenterURL'));
    // </if>
    // <if expr="not is_chromeos">
    assertFalse(!!row, 'Chrome Root Store Help Center link unexpectedly found');
    // </if>
  });

  // <if expr="not chromeos_lacros">
  // TODO(crbug.com/1148302): This class directly calls
  // `CreateNSSCertDatabaseGetterForIOThread()` that causes crash at the
  // moment and is never called from Lacros-Chrome. This should be revisited
  // when there is a solution for the client certificates settings page on
  // Lacros-Chrome.
  test('LogManageCertificatesClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#manageCertificatesLinkRow')!.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.MANAGE_CERTIFICATES, result);
  });
  // </if>

  test('ManageSecurityKeysSubpageVisible', function() {
    assertTrue(isChildVisible(page, '#security-keys-subpage-trigger'));
  });

  test('ManageSecurityKeysPhonesSubpageHidden', function() {
    assertFalse(isChildVisible(page, '#security-keys-phones-subpage-trigger'));
  });

  // Tests that changing the HTTPS-First Mode setting sets the associated pref.
  test('HttpsFirstModeRadioButtons', async () => {
    let radioButton = page.shadowRoot!.querySelector<HTMLElement>(
        '#httpsFirstModeEnabledFull');
    const radioGroup = page.shadowRoot!.querySelector<HTMLElement>(
        '#httpsFirstModeRadioGroup');
    assertTrue(!!radioButton);
    assertTrue(!!radioGroup);
    radioButton.click();
    await eventToPromise('selected-changed', radioGroup);
    assertEquals(
        HttpsFirstModeSetting.ENABLED_FULL,
        page.getPref('generated.https_first_mode_enabled').value);

    radioButton = page.shadowRoot!.querySelector<HTMLElement>(
        '#httpsFirstModeEnabledIncognito');
    assertTrue(!!radioButton);
    radioButton.click();
    await eventToPromise('selected-changed', radioGroup);
    assertEquals(
        HttpsFirstModeSetting.ENABLED_INCOGNITO,
        page.getPref('generated.https_first_mode_enabled').value);

    radioButton =
        page.shadowRoot!.querySelector<HTMLElement>('#httpsFirstModeDisabled');
    assertTrue(!!radioButton);
    radioButton.click();
    await eventToPromise('selected-changed', radioGroup);
    assertEquals(
        HttpsFirstModeSetting.DISABLED,
        page.getPref('generated.https_first_mode_enabled').value);
  });

  // Test that clicking the V8 security row navigates to the setting page.
  test('NavigateToV8Setting', function() {
    const link =
        page.shadowRoot!.querySelector<HTMLElement>('#v8-setting-link');
    assertTrue(!!link);
    link.click();
    assertEquals(
        routes.SITE_SETTINGS_JAVASCRIPT_JIT,
        Router.getInstance().getCurrentRoute());
  });

  // TODO(crbug.com/1494186): Add test for alternate sub-label when Advanced
  // Protection is enabled.
});

suite('SecurityPageHappinessTrackingSurveys', function() {
  let testHatsBrowserProxy: TestHatsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-security-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    testHatsBrowserProxy.reset();
    Router.getInstance().navigateTo(routes.SECURITY);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('SecurityPageSwitchRouteCallsHatsProxy', async function() {
    const t1 = 10000;
    testHatsBrowserProxy.setNow(t1);
    window.dispatchEvent(new Event('focus'));

    const t2 = 20000;
    testHatsBrowserProxy.setNow(t2);
    window.dispatchEvent(new Event('blur'));

    // Switch tabs within the settings page.
    Router.getInstance().navigateTo(routes.PRIVACY);

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    // Verify that the method securityPageHatsRequest was called and the time
    // the user spent on the security page was logged correctly.
    const expectedTotalTimeInFocus = t2 - t1;
    assertEquals(expectedTotalTimeInFocus, args[2]);
  });

  test('SecurityPageBeforeUnloadCallsHatsProxy', async function() {
    // Interact with the security page.
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    const t1 = 10000;
    testHatsBrowserProxy.setNow(t1);
    window.dispatchEvent(new Event('focus'));

    const t2 = 20000;
    testHatsBrowserProxy.setNow(t2);
    window.dispatchEvent(new Event('blur'));

    const t3 = 60000;
    testHatsBrowserProxy.setNow(t3);
    window.dispatchEvent(new Event('focus'));

    const t4 = 80000;
    testHatsBrowserProxy.setNow(t4);
    window.dispatchEvent(new Event('blur'));

    // Fire the beforeunload event to simulate closing the page.
    window.dispatchEvent(new Event('beforeunload'));

    const args =
        await testHatsBrowserProxy.whenCalled('securityPageHatsRequest');

    // Verify the latest interaction type.
    assertEquals(SecurityPageInteraction.RADIO_BUTTON_ENHANCED_CLICK, args[0]);

    // Verify the safe browsing state on open.
    assertEquals(SafeBrowsingSetting.STANDARD, args[1]);

    // Verify the time the user spend on the security page.
    const expectedTotalTimeInFocus = t2 - t1 + t4 - t3;
    assertEquals(expectedTotalTimeInFocus, args[2]);
  });
});

suite('FlagsDisabled', function() {
  let page: SettingsSecurityPageElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testPrivacyBrowserProxy: TestPrivacyPageBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: false,
      enableFriendlierSafeBrowsingSettings: false,
      enableHashPrefixRealTimeLookups: false,
      enableHttpsFirstModeNewSettings: false,
      enableCertManagementUIV2: false,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testPrivacyBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-security-page');
    page.prefs = pagePrefs();
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  // <if expr="is_macosx or is_win">
  test('NativeCertificateManager', function() {
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#manageCertificatesLinkRow')!.click();
    return testPrivacyBrowserProxy.whenCalled('showManageSslCertificates');
  });
  // </if>

  test('ChromeRootStorePage', async function() {
    const row =
        page.shadowRoot!.querySelector<HTMLElement>('#chromeCertificates');
    assertTrue(!!row);
    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('chromeRootStoreHelpCenterURL'));
  });

  // <if expr="not chromeos_lacros">
  // TODO(crbug.com/1148302): This class directly calls
  // `CreateNSSCertDatabaseGetterForIOThread()` that causes crash at the
  // moment and is never called from Lacros-Chrome. This should be revisited
  // when there is a solution for the client certificates settings page on
  // Lacros-Chrome.
  test('LogManageCertificatesClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#manageCertificatesLinkRow')!.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.MANAGE_CERTIFICATES, result);
  });
  // </if>

  test('ManageSecurityKeysSubpageHidden', function() {
    assertFalse(isChildVisible(page, '#security-keys-subpage-trigger'));
  });

  // The element only exists on Windows.
  // <if expr="is_win">
  test('ManageSecurityKeysPhonesSubpageVisibleAndNavigates', function() {
    // On modern versions of Windows the security keys subpage will be disabled
    // because Windows manages that itself, but a link to the subpage for
    // managing phones as security keys will be included.
    const triggerId = '#security-keys-phones-subpage-trigger';
    assertTrue(isChildVisible(page, triggerId));
    page.shadowRoot!.querySelector<HTMLElement>(triggerId)!.click();
    flush();
    assertEquals(
        routes.SECURITY_KEYS_PHONES, Router.getInstance().getCurrentRoute());
  });
  // </if>

  // TODO(crbug.com/1466292): Remove once friendlier safe browsing settings
  // standard protection is launched.
  test('NotUpdatedStandardProtectionDropdown', function() {
    const standardProtection = page.$.safeBrowsingStandard;
    const spSubLabel = loadTimeData.getString('safeBrowsingStandardDesc');
    assertEquals(spSubLabel, standardProtection.subLabel);

    const safeBrowsingStandardBulTwo =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#safeBrowsingStandardBulTwo')!;
    const subBulTwoLabel = loadTimeData.getString('safeBrowsingStandardBulTwo');
    assertEquals(
        subBulTwoLabel, safeBrowsingStandardBulTwo.textContent!.trim());

    const passwordsLeakToggle = page.$.passwordsLeakToggle;
    const passwordLeakLabel =
        loadTimeData.getString('passwordsLeakDetectionLabel');
    assertEquals(passwordLeakLabel, passwordsLeakToggle.label);

    const passwordLeakSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription');
    assertEquals(passwordLeakSubLabel, passwordsLeakToggle.subLabel);
  });

  // TODO(crbug.com/1470385): Remove once friendlier safe browsing settings
  // enhanced protection is launched.
  test('NotUpdatedEnhancedProtectionDropdown', function() {
    // Enhanced protection sublabel should not be the updated one.
    const enhancedProtection = page.$.safeBrowsingEnhanced;
    const epSubLabel = loadTimeData.getString('safeBrowsingEnhancedDesc');
    assertEquals(epSubLabel, enhancedProtection.subLabel);

    // The updated description container should not be visible.
    assertFalse(isChildVisible(page, '#enhancedProtectionDescContainer'));

    // No protection sublabel should not be the updated one.
    const noProtection = page.$.safeBrowsingDisabled;
    const npSubLabel = loadTimeData.getString('safeBrowsingNoneDesc');
    assertEquals(npSubLabel, noProtection.subLabel);
  });

  // TODO(crbug.com/1466292): Remove once friendlier safe browsing settings
  // standard protection is launched.
  test('NotUpdatedPasswordsLeakDetectionSubLabel', function() {
    const toggle = page.$.passwordsLeakToggle;
    const defaultSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription');
    const activeWhenSignedInSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription') +
        ' ' +
        loadTimeData.getString(
            'passwordsLeakDetectionSignedOutEnabledDescription');
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', true);
    page.set(
        'prefs.generated.password_leak_detection.userControlDisabled', true);
    flush();
    assertEquals(activeWhenSignedInSubLabel, toggle.subLabel);

    page.set('prefs.generated.password_leak_detection.value', true);
    page.set(
        'prefs.generated.password_leak_detection.userControlDisabled', false);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', false);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);
  });

  // Tests that toggling the HTTPS-Only Mode setting sets the associated pref.
  test('HttpsOnlyModeToggle', function() {
    const httpsOnlyModeToggle =
        page.shadowRoot!.querySelector<HTMLElement>('#httpsOnlyModeToggle');
    assertTrue(!!httpsOnlyModeToggle);

    assertEquals(
        HttpsFirstModeSetting.DISABLED,
        page.getPref('generated.https_first_mode_enabled').value);

    httpsOnlyModeToggle.click();
    assertEquals(
        HttpsFirstModeSetting.ENABLED_FULL,
        page.getPref('generated.https_first_mode_enabled').value);
  });

  // Tests that the correct Advanced Protection sublabel is used when the
  // HTTPS-Only Mode setting toggle has user control disabled.
  test('HttpsOnlyModeSettingSubLabel', function() {
    const toggle = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#httpsOnlyModeToggle');
    assertTrue(!!toggle);
    const defaultSubLabel = loadTimeData.getString('httpsOnlyModeDescription');
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.setPrefValue(
        'generated.https_first_mode_enabled', HttpsFirstModeSetting.DISABLED);
    page.set(
        'prefs.generated.https_first_mode_enabled.userControlDisabled', true);
    flush();
    const lockedSubLabel =
        loadTimeData.getString('httpsOnlyModeDescriptionAdvancedProtection');
    assertEquals(lockedSubLabel, toggle.subLabel);

    page.setPrefValue(
        'generated.https_first_mode_enabled',
        HttpsFirstModeSetting.ENABLED_FULL);
    page.set(
        'prefs.generated.https_first_mode_enabled.userControlDisabled', true);
    flush();
    assertEquals(lockedSubLabel, toggle.subLabel);
  });
});

// Separate test suite for tests specifically related to Safe Browsing controls.
suite('SafeBrowsing', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testPrivacyBrowserProxy: TestPrivacyPageBrowserProxy;
  let page: SettingsSecurityPageElement;
  let openWindowProxy: TestOpenWindowProxy;

  async function setUpPage() {
    page = document.createElement('settings-security-page');
    page.prefs = pagePrefs();
    document.body.appendChild(page);
    page.$.safeBrowsingEnhanced.updateCollapsed();
    page.$.safeBrowsingStandard.updateCollapsed();
    flush();
  }
  async function resetPage() {
    page.remove();
    await setUpPage();
  }

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testPrivacyBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    return setUpPage();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  // Initially specified pref option should be expanded
  test('SafeBrowsingRadio_InitialPrefOptionIsExpanded', function() {
    assertFalse(page.$.safeBrowsingEnhanced.expanded);
    assertTrue(page.$.safeBrowsingStandard.expanded);
  });

  test('PasswordsLeakDetectionSubLabel', function() {
    const toggle = page.$.passwordsLeakToggle;
    const defaultSubLabel = loadTimeData.getString(
        'passwordsLeakDetectionGeneralDescriptionUpdated');
    const activeWhenSignedInSubLabel =
        loadTimeData.getString(
            'passwordsLeakDetectionGeneralDescriptionUpdated') +
        ' ' +
        loadTimeData.getString(
            'passwordsLeakDetectionSignedOutEnabledDescription');
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', true);
    page.set(
        'prefs.generated.password_leak_detection.userControlDisabled', true);
    flush();
    assertEquals(activeWhenSignedInSubLabel, toggle.subLabel);

    page.set('prefs.generated.password_leak_detection.value', true);
    page.set(
        'prefs.generated.password_leak_detection.userControlDisabled', false);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', false);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);
  });

  test('LogSafeBrowsingExtendedToggle', async function() {
    page.$.safeBrowsingStandard.click();
    flush();

    page.$.safeBrowsingReportingToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.IMPROVE_SECURITY, result);
  });

  test('safeBrowsingReportingToggle', async () => {
    page.$.safeBrowsingStandard.click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    const safeBrowsingReportingToggle = page.$.safeBrowsingReportingToggle;
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);

    // This could also be set to disabled, anything other than standard.
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();
    assertTrue(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);

    page.$.safeBrowsingStandard.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
  });

  test(
      'SafeBrowsingRadio_ManuallyExpandedRemainExpandedOnRepeatSelection',
      async function() {
        page.$.safeBrowsingStandard.click();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertFalse(page.$.safeBrowsingEnhanced.expanded);

        // Expanding another radio button should not collapse already expanded
        // option.
        page.$.safeBrowsingEnhanced.$.expandButton.click();
        await page.$.safeBrowsingEnhanced.$.expandButton.updateComplete;
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);

        // Clicking on already selected button should not collapse manually
        // expanded option.
        page.$.safeBrowsingStandard.click();
        // Wait one cycle and confirm nothing changed.
        await microtasksFinished();
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);
      });

  test(
      'SafeBrowsingRadio_ManuallyExpandedRemainExpandedOnSelectedChanged',
      async function() {
        page.$.safeBrowsingStandard.click();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);

        page.$.safeBrowsingEnhanced.$.expandButton.click();
        await page.$.safeBrowsingEnhanced.$.expandButton.updateComplete;
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);

        page.$.safeBrowsingDisabled.click();
        await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

        // Previously selected option must remain opened.
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);

        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
            .confirm.click();
        flush();

        // Wait for onDisableSafebrowsingDialogClose_ to finish.
        await flushTasks();

        // The deselected option should become collapsed.
        assertFalse(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);
      });

  test('DisableSafebrowsingDialog_Confirm', async function() {
    page.$.safeBrowsingStandard.click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .confirm.click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertFalse(isChildVisible(page, 'settings-simple-confirmation-dialog'));

    assertFalse(page.$.safeBrowsingEnhanced.checked);
    assertFalse(page.$.safeBrowsingStandard.checked);
    assertTrue(page.$.safeBrowsingDisabled.checked);
    assertEquals(
        SafeBrowsingSetting.DISABLED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromEnhanced', async function() {
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);

    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingEnhanced.expanded);

    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .cancel.click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertFalse(isChildVisible(page, 'settings-simple-confirmation-dialog'));

    assertTrue(page.$.safeBrowsingEnhanced.checked);
    assertFalse(page.$.safeBrowsingStandard.checked);
    assertFalse(page.$.safeBrowsingDisabled.checked);
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromStandard', async function() {
    page.$.safeBrowsingStandard.click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .cancel.click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertFalse(isChildVisible(page, 'settings-simple-confirmation-dialog'));

    assertFalse(page.$.safeBrowsingEnhanced.checked);
    assertTrue(page.$.safeBrowsingStandard.checked);
    assertFalse(page.$.safeBrowsingDisabled.checked);
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
  });

  test('noControlSafeBrowsingReportingInEnhanced', async () => {
    page.$.safeBrowsingStandard.click();
    assertFalse(page.$.safeBrowsingReportingToggle.disabled);
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    assertTrue(page.$.safeBrowsingReportingToggle.disabled);
  });

  test('noValueChangeSafeBrowsingReportingInEnhanced', async () => {
    page.$.safeBrowsingStandard.click();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noControlSafeBrowsingReportingInDisabled', async function() {
    page.$.safeBrowsingStandard.click();

    assertFalse(page.$.safeBrowsingReportingToggle.disabled);
    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .confirm.click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(page.$.safeBrowsingReportingToggle.disabled);
  });

  test('noValueChangeSafeBrowsingReportingInDisabled', async function() {
    page.$.safeBrowsingStandard.click();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .confirm.click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noValueChangePasswordLeakSwitchToEnhanced', async () => {
    page.$.safeBrowsingStandard.click();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('noValuePasswordLeakSwitchToDisabled', async function() {
    page.$.safeBrowsingStandard.click();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .confirm.click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('safeBrowsingUserActionRecorded', async function() {
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    page.$.safeBrowsingStandard.click();
    // Wait for a timeout to ensure the call count check below catches any
    // possible incorrect calls.
    await microtasksFinished();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    // Not logged because it is already in standard mode.
    assertEquals(
        0,
        testMetricsBrowserProxy.getCallCount(
            'recordSafeBrowsingInteractionHistogram'));

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    const [enhancedClickedResult, enhancedClickedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction'),
    ]);
    assertEquals(
        SafeBrowsingInteractions.SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED,
        enhancedClickedResult);
    assertEquals(
        'SafeBrowsing.Settings.EnhancedProtectionClicked',
        enhancedClickedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.$.safeBrowsingEnhanced.$.expandButton.click();
    flush();
    const [enhancedExpandedResult, enhancedExpandedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction'),
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED,
        enhancedExpandedResult);
    assertEquals(
        'SafeBrowsing.Settings.EnhancedProtectionExpandArrowClicked',
        enhancedExpandedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.$.safeBrowsingStandard.$.expandButton.click();
    flush();
    const [standardExpandedResult, standardExpandedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction'),
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED,
        standardExpandedResult);
    assertEquals(
        'SafeBrowsing.Settings.StandardProtectionExpandArrowClicked',
        standardExpandedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    const [disableClickedResult, disableClickedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction'),
    ]);
    assertEquals(
        SafeBrowsingInteractions.SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED,
        disableClickedResult);
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingClicked',
        disableClickedAction);

    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .cancel.click();
    flush();
    const [disableDeniedResult, disableDeniedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction'),
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED,
        disableDeniedResult);
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingDialogDenied',
        disableDeniedAction);

    await flushTasks();

    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.shadowRoot!.querySelector('settings-simple-confirmation-dialog')!.$
        .confirm.click();
    flush();
    const [disableConfirmedResult, disableConfirmedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction'),
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED,
        disableConfirmedResult);
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingDialogConfirmed',
        disableConfirmedAction);
  });

  test('securityPageShowedRecorded', async function() {
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
    assertEquals(
        SafeBrowsingInteractions.SAFE_BROWSING_SHOWED,
        await testMetricsBrowserProxy.whenCalled(
            'recordSafeBrowsingInteractionHistogram'));
  });

  test('standardProtectionExpandedIfNoQueryParam', function() {
    // Standard protection should be pre-expanded if there is no param.
    Router.getInstance().navigateTo(routes.SECURITY);
    assertEquals(
        page.prefs.generated.safe_browsing.value, SafeBrowsingSetting.STANDARD);
    assertFalse(page.$.safeBrowsingEnhanced.expanded);
    assertTrue(page.$.safeBrowsingStandard.expanded);
  });

  test('enhancedProtectionCollapsedIfParamIsEnhanced', function() {
    // Enhanced protection should be collapsed if the param is set to
    // enhanced.
    Router.getInstance().navigateTo(
        routes.SECURITY,
        /* dynamicParams= */ new URLSearchParams('q=enhanced'));
    assertEquals(
        page.prefs.generated.safe_browsing.value, SafeBrowsingSetting.STANDARD);
    assertFalse(page.$.safeBrowsingEnhanced.expanded);
    assertFalse(page.$.safeBrowsingStandard.expanded);
  });

  test('UpdatedStandardProtectionDropdown', async () => {
    loadTimeData.overrideValues({
      enableHashPrefixRealTimeLookups: false,
    });
    await resetPage();
    const standardProtection = page.$.safeBrowsingStandard;
    const updatedSpSubLabel =
        loadTimeData.getString('safeBrowsingStandardDescUpdated');
    assertEquals(updatedSpSubLabel, standardProtection.subLabel);

    const passwordsLeakToggle = page.$.passwordsLeakToggle;
    const updatedPasswordLeakLabel =
        loadTimeData.getString('passwordsLeakDetectionLabelUpdated');
    assertEquals(updatedPasswordLeakLabel, passwordsLeakToggle.label);

    const updatedPasswordLeakSubLabel = loadTimeData.getString(
        'passwordsLeakDetectionGeneralDescriptionUpdated');
    assertEquals(updatedPasswordLeakSubLabel, passwordsLeakToggle.subLabel);
  });

  test('UpdatedEnhancedProtectionText', async () => {
    const enhancedProtection = page.$.safeBrowsingEnhanced;
    const epSubLabel =
        loadTimeData.getString('safeBrowsingEnhancedDescUpdated');
    assertEquals(epSubLabel, enhancedProtection.subLabel);

    const noProtection = page.$.safeBrowsingDisabled;
    const npSubLabel = loadTimeData.getString('safeBrowsingNoneDescUpdated');
    assertEquals(npSubLabel, noProtection.subLabel);

    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    // Learn more label should be visible.
    assertTrue(isChildVisible(page, '#learnMoreLabelContainer'));
  });

  test('LearnMoreLinkClickableWhenControlledByPolicy', async () => {
    page.$.safeBrowsingEnhanced.$.expandButton.click();

    // Set the page to be enterprise policy enforced.
    page.set(
        'prefs.generated.safe_browsing.enforcement',
        chrome.settingsPrivate.Enforcement.ENFORCED);
    flush();

    const learnMoreLink = page.shadowRoot!.querySelector<HTMLElement>(
        '#enhancedProtectionLearnMoreLink');

    // Confirm that the learnMoreLink element exists.
    assertNotEquals(learnMoreLink, null);

    // Confirm that the pointer-events value is auto when enterprise policy is
    // enforced.
    assertEquals(
        'auto',
        (learnMoreLink!.computedStyleMap()!.get('pointer-events') as
         CSSKeywordValue)
            .value);

    // Confirm that the correct link was clicked.
    learnMoreLink!.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        url, loadTimeData.getString('enhancedProtectionHelpCenterURL'));
  });

  // <if expr="_google_chrome">
  test('StandardProtectionDropdownWithProxyString', async () => {
    loadTimeData.overrideValues({
      enableHashPrefixRealTimeLookups: true,
    });
    await resetPage();
    const standardProtection = page.$.safeBrowsingStandard;
    const subLabel =
        loadTimeData.getString('safeBrowsingStandardDescUpdatedProxy');
    assertEquals(subLabel, standardProtection.subLabel);
  });

  // TODO(crbug.com/1466292): Remove once friendlier safe browsing settings
  // standard protection is launched.
  test(
      'FriendlierSettingsDisabledStandardProtectionDropdownWithProxyString',
      async () => {
        loadTimeData.overrideValues({
          enableFriendlierSafeBrowsingSettings: false,
          enableHashPrefixRealTimeLookups: true,
        });
        await resetPage();
        const standardProtection = page.$.safeBrowsingStandard;
        const subLabel = loadTimeData.getString('safeBrowsingStandardDesc');
        assertEquals(subLabel, standardProtection.subLabel);
        const safeBrowsingStandardBulTwo =
            page.shadowRoot!.querySelector<HTMLElement>(
                '#safeBrowsingStandardBulTwo')!;
        const subBulTwoLabel =
            loadTimeData.getString('safeBrowsingStandardBulTwoProxy');
        assertEquals(
            subBulTwoLabel, safeBrowsingStandardBulTwo.textContent!.trim());
      });
  // </if>
  // <if expr="not _google_chrome">
  test('StandardProtectionDropdownNoProxyStringForChromium', function() {
    // If this test fails, it may be because hash-prefix real-time lookups have
    // been enabled for Chromium. The settings strings only currently support
    // Chrome, so this must be addressed to support Chromium as well.
    const standardProtection = page.$.safeBrowsingStandard;
    const subLabel = loadTimeData.getString('safeBrowsingStandardDescUpdated');
    assertEquals(subLabel, standardProtection.subLabel);
  });
  // </if>

  test('FriendlierSettingsPopulatedOnEsbOptIn', async function() {
    loadTimeData.overrideValues({
      enableFriendlierSafeBrowsingSettings: false,
    });
    await resetPage();
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    assertFalse(
        page.getPref('safebrowsing.esb_opt_in_with_friendlier_settings').value);

    loadTimeData.overrideValues({
      enableFriendlierSafeBrowsingSettings: true,
    });
    await resetPage();
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    assertTrue(
        page.getPref('safebrowsing.esb_opt_in_with_friendlier_settings').value);
  });

  test('FriendlierSettingsClearedOnEsbOptOut', async function() {
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    page.setPrefValue('safebrowsing.esb_opt_in_with_friendlier_settings', true);
    page.$.safeBrowsingStandard.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);
    assertFalse(
        page.getPref('safebrowsing.esb_opt_in_with_friendlier_settings').value);
  });
});
