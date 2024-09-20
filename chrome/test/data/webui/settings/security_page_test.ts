// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSecurityPageElement} from 'chrome://settings/lazy_load.js';
import {HttpsFirstModeSetting, SafeBrowsingSetting} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {HatsBrowserProxyImpl, CrSettingsPrefs, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrivacyElementInteractions, PrivacyPageBrowserProxyImpl, resetRouterForTesting, Router, routes, SafeBrowsingInteractions, SecureDnsMode, SecurityPageInteraction} from 'chrome://settings/settings.js';
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
    resetRouterForTesting();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testPrivacyBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

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
    // Chrome Root Store Help link should not be present since
    // kEnableCertManagementUIV2 feature flag is enabled by
    // SettingsSecurityPageTest constructor.
    // TODO(crbug.com/40928765): remove this comment once the feature flag is
    // set to default enabled.
    const row =
        page.shadowRoot!.querySelector<HTMLElement>('#chromeCertificates');
    assertFalse(!!row, 'Chrome Root Store Help Center link unexpectedly found');
  });

  // <if expr="not chromeos_lacros">
  // TODO(crbug.com/40156980): This class directly calls
  // `CreateNSSCertDatabaseGetterForIOThread()` that causes crash at the
  // moment and is never called from Lacros-Chrome. This should be revisited
  // when there is a solution for the client certificates settings page on
  // Lacros-Chrome.
  test('ManageCertificatesClick', async function() {
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#manageCertificatesLinkRow')!.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.MANAGE_CERTIFICATES, result);

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('certManagementV2URL'));
  });
  // </if>

  test('ManageSecurityKeysSubpageVisible', function() {
    assertTrue(isChildVisible(page, '#security-keys-subpage-trigger'));
  });

  test('ManageSecurityKeysPhonesSubpageHidden', function() {
    assertFalse(isChildVisible(page, '#security-keys-phones-subpage-trigger'));
  });

  // Tests that changing the HTTPS-First Mode setting sets the associated pref,
  // and that the radio options are correctly shown/hidden based on the top
  // level toggle.
  test('HttpsFirstModeControls', async () => {
    // Check that the old toggle row under "Advanced" is _not_ present.
    const oldToggle =
        page.shadowRoot!.querySelector<HTMLElement>('#httpsOnlyModeToggle');
    assertFalse(!!oldToggle);

    // Test the new settings UI.
    const secureConnections = page.shadowRoot!.querySelector<HTMLElement>(
        '#secureConnectionsSection');
    const toggleButton =
        page.shadowRoot!.querySelector<HTMLElement>('#httpsFirstModeToggle');
    const collapse = page.shadowRoot!.querySelector<HTMLElement>(
        '#httpsFirstModeRadioGroupCollapse');
    const radioGroup = page.shadowRoot!.querySelector<HTMLElement>(
        '#httpsFirstModeRadioGroup');
    assertTrue(!!secureConnections);
    assertTrue(!!toggleButton);
    assertTrue(!!collapse);
    assertTrue(!!radioGroup);

    assertEquals(
        HttpsFirstModeSetting.DISABLED,
        page.getPref('generated.https_first_mode_enabled').value);
    assertFalse(isChildVisible(page, '#httpsFirstModeRadioGroup'));

    // Toggling on the button should (1) expand the cr-collapse, and (2) select
    // the "balanced mode" radio button and set the pref to "balanced".
    toggleButton.click();
    await eventToPromise('transitionend', collapse);
    assertTrue(isChildVisible(page, '#httpsFirstModeRadioGroup'));
    assertEquals(
        HttpsFirstModeSetting.ENABLED_BALANCED,
        page.getPref('generated.https_first_mode_enabled').value);

    // Select the "Strict Mode" radio button.
    let radioButton = page.shadowRoot!.querySelector<HTMLElement>(
        '#httpsFirstModeEnabledStrict');
    assertTrue(!!radioButton);
    radioButton.click();
    await eventToPromise('selected-changed', radioGroup);
    assertEquals(
        HttpsFirstModeSetting.ENABLED_FULL,
        page.getPref('generated.https_first_mode_enabled').value);

    // Select the "Balanced Mode" radio button again.
    radioButton = page.shadowRoot!.querySelector<HTMLElement>(
        '#httpsFirstModeEnabledBalanced');
    assertTrue(!!radioButton);
    radioButton.click();
    await eventToPromise('selected-changed', radioGroup);
    assertEquals(
        HttpsFirstModeSetting.ENABLED_BALANCED,
        page.getPref('generated.https_first_mode_enabled').value);

    // Toggling on the button off should (1) hide the cr-collapse, and (2) fully
    // turn off HTTPS-First Mode.
    toggleButton.click();
    await eventToPromise('transitionend', collapse);
    assertFalse(isChildVisible(page, '#httpsFirstModeRadioGroup'));
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
        routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER,
        Router.getInstance().getCurrentRoute());
  });

  // Tests that the correct Advanced Protection sublabel is used when the
  // HTTPS-First Mode setting toggle has user control disabled.
  test('HttpsFirstModeSettingAdvancedProtectionSubLabel', function() {
    const toggle = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#httpsFirstModeToggle');
    assertTrue(!!toggle);
    const defaultSubLabel =
        loadTimeData.getString('httpsFirstModeSectionDescription');
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.setPrefValue(
        'generated.https_first_mode_enabled', HttpsFirstModeSetting.DISABLED);
    page.set(
        'prefs.generated.https_first_mode_enabled.userControlDisabled', true);
    flush();
    const lockedSubLabel =
        loadTimeData.getString('httpsFirstModeDescriptionAdvancedProtection');
    assertEquals(lockedSubLabel, toggle.subLabel);

    page.setPrefValue(
        'generated.https_first_mode_enabled',
        HttpsFirstModeSetting.ENABLED_FULL);
    page.set(
        'prefs.generated.https_first_mode_enabled.userControlDisabled', true);
    flush();
    assertEquals(lockedSubLabel, toggle.subLabel);
  });

  // Tests that only the new Secure DNS toggle is shown when the new
  // HTTPS-First Mode Settings flag is enabled.
  // Regression test for crbug.com/365884462
  // TODO(crbug.com/349860796): Remove when Balanced Mode is fully launched.
  // <if expr="not is_chromeos">
  test('SecureDnsToggleNotDuplicated', function() {
    // Check that the setting under the new element ID visible.
    assertTrue(isChildVisible(page, '#secureDnsSettingNew'));

    // Check that the setting under the old element ID is not visible.
    assertFalse(isChildVisible(page, '#secureDnsSettingOld'));
  });
  // </if>
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
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

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
      enableHashPrefixRealTimeLookups: false,
      enableHttpsFirstModeNewSettings: false,
      enableCertManagementUIV2: false,
      extendedReportingRemovePrefDependency: false,
      hashPrefixRealTimeLookupsSamplePing: false,
    });
    resetRouterForTesting();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testPrivacyBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    page = document.createElement('settings-security-page');
    page.prefs = pagePrefs();
    document.body.appendChild(page);

    page.$.safeBrowsingEnhanced.updateCollapsed();
    page.$.safeBrowsingStandard.updateCollapsed();
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
  // TODO(crbug.com/40156980): This class directly calls
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

  // Tests the old HTTPS-Only Mode toggle UI.
  // TODO(crbug.com/349860796): Remove this test once HttpsFirstBalancedMode is
  // enabled by default.
  test('HttpsOnlyModeToggle', function() {
    // Check that the new "Secure connections" section is not shown if the
    // flag is disabled.
    const secureConnections =
        page.shadowRoot!.querySelector<HTMLElement>('secureConnectionsSection');
    assertFalse(!!secureConnections);

    // Test the old settings UI when the HttpsFirstBalancedMode flag is
    // disabled. Checks that toggling the HTTPS-Only Mode setting sets the
    // associated pref.
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

  // Tests that only the old Secure DNS toggle is shown when the new
  // HTTPS-First Mode Settings flag is disabled.
  // Regression test for crbug.com/365884462
  // TODO(crbug.com/349860796): Remove when Balanced Mode is fully launched.
  // <if expr="not is_chromeos">
  test('SecureDnsToggleNotDuplicated', function() {
    // Check that the setting under the new element ID is not visible.
    assertFalse(isChildVisible(page, '#secureDnsSettingNew'));

    // Check that the setting under the old element ID is visible.
    assertTrue(isChildVisible(page, '#secureDnsSettingOld'));
  });
  // </if>

  // TODO(crbug.com/349439367): Remove the test once
  // kExtendedReportingRemovePrefDependency is fully launched.
  test('LogSafeBrowsingExtendedToggle', async function() {
    const safeBrowsingReportingToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#safeBrowsingReportingToggle');
    assertTrue(!!safeBrowsingReportingToggle);
    page.$.safeBrowsingStandard.click();
    flush();

    safeBrowsingReportingToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.IMPROVE_SECURITY, result);
  });

  // TODO(crbug.com/349439367): Remove the test once
  // kExtendedReportingRemovePrefDependency is fully launched.
  test('safeBrowsingReportingToggle', async () => {
    page.$.safeBrowsingStandard.click();
    await microtasksFinished();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    const safeBrowsingReportingToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#safeBrowsingReportingToggle');
    assertTrue(!!safeBrowsingReportingToggle);
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);

    // This could also be set to disabled, anything other than standard.
    page.$.safeBrowsingEnhanced.click();
    await microtasksFinished();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();
    assertTrue(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);

    page.$.safeBrowsingStandard.click();
    await microtasksFinished();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
  });

  // TODO(crbug.com/349439367): Remove the test once
  // kExtendedReportingRemovePrefDependency is fully launched.
  test('noControlSafeBrowsingReportingInEnhanced', async () => {
    const safeBrowsingReportingToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#safeBrowsingReportingToggle');
    assertTrue(!!safeBrowsingReportingToggle);
    page.$.safeBrowsingStandard.click();
    assertFalse(safeBrowsingReportingToggle.disabled);
    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    assertTrue(safeBrowsingReportingToggle.disabled);
  });

  // TODO(crbug.com/349439367): Remove the test once
  // kExtendedReportingRemovePrefDependency is fully launched.
  test('noControlSafeBrowsingReportingInDisabled', async function() {
    const safeBrowsingReportingToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#safeBrowsingReportingToggle');
    assertTrue(!!safeBrowsingReportingToggle);
    page.$.safeBrowsingStandard.click();
    await microtasksFinished();

    assertFalse(safeBrowsingReportingToggle.disabled);
    page.$.safeBrowsingDisabled.click();
    await microtasksFinished();

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    await clickConfirmOnDisableSafebrowsingDialog(page);

    assertTrue(safeBrowsingReportingToggle.disabled);
  });

  // TODO(crbug.com/349439367): Remove the test once
  // kExtendedReportingRemovePrefDependency is fully launched.
  test(
      'safeBrowsingReportingToggleVisibleWhenExtendedReportingNotDeprecated',
      async function() {
        // The safeBrowsingReportingToggle should be visible if the extended
        // reporting deprecation flag is not enabled.
        page.$.safeBrowsingStandard.click();
        flush();

        await microtasksFinished();
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertTrue(isChildVisible(page, '#safeBrowsingReportingToggle'));
      });
});

// Separate test suite for tests specifically related to Safe Browsing controls.
suite('SafeBrowsing', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testPrivacyBrowserProxy: TestPrivacyPageBrowserProxy;
  let page: SettingsSecurityPageElement;
  let openWindowProxy: TestOpenWindowProxy;

  function setUpPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testPrivacyBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    page = document.createElement('settings-security-page');
    page.prefs = pagePrefs();
    document.body.appendChild(page);
    page.$.safeBrowsingEnhanced.updateCollapsed();
    page.$.safeBrowsingStandard.updateCollapsed();
    return microtasksFinished();
  }

  async function resetPage() {
    page.remove();
    await setUpPage();
  }

  setup(function() {
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

  test(
      'SafeBrowsingRadio_ManuallyExpandedRemainExpandedOnRepeatSelection',
      async function() {
        page.$.safeBrowsingStandard.click();
        await microtasksFinished();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertFalse(page.$.safeBrowsingEnhanced.expanded);

        // Expanding another radio button should not collapse already expanded
        // option.
        page.$.safeBrowsingEnhanced.$.expandButton.click();
        await microtasksFinished();
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
        await microtasksFinished();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);

        page.$.safeBrowsingEnhanced.$.expandButton.click();
        await microtasksFinished();
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);

        page.$.safeBrowsingDisabled.click();
        await microtasksFinished();

        // Previously selected option must remain opened.
        assertTrue(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);
        await clickConfirmOnDisableSafebrowsingDialog(page);

        // The deselected option should become collapsed.
        assertFalse(page.$.safeBrowsingStandard.expanded);
        assertTrue(page.$.safeBrowsingEnhanced.expanded);
      });

  test('DisableSafebrowsingDialog_Confirm', async function() {
    page.$.safeBrowsingStandard.click();
    await microtasksFinished();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    page.$.safeBrowsingDisabled.click();
    await microtasksFinished();

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    await clickConfirmOnDisableSafebrowsingDialog(page);

    assertFalse(page.$.safeBrowsingEnhanced.checked);
    assertFalse(page.$.safeBrowsingStandard.checked);
    assertTrue(page.$.safeBrowsingDisabled.checked);
    assertEquals(
        SafeBrowsingSetting.DISABLED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromEnhanced', async function() {
    page.$.safeBrowsingEnhanced.click();
    await microtasksFinished();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);

    page.$.safeBrowsingDisabled.click();
    await microtasksFinished();

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingEnhanced.expanded);
    await clickCancelOnDisableSafebrowsingDialog(page);

    assertTrue(page.$.safeBrowsingEnhanced.checked);
    assertFalse(page.$.safeBrowsingStandard.checked);
    assertFalse(page.$.safeBrowsingDisabled.checked);
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromStandard', async function() {
    page.$.safeBrowsingStandard.click();
    await microtasksFinished();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    page.$.safeBrowsingDisabled.click();
    await microtasksFinished();

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);
    await clickCancelOnDisableSafebrowsingDialog(page);

    assertFalse(page.$.safeBrowsingEnhanced.checked);
    assertTrue(page.$.safeBrowsingStandard.checked);
    assertFalse(page.$.safeBrowsingDisabled.checked);
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
  });

  test('noValueChangeSafeBrowsingReportingInEnhanced', async () => {
    page.$.safeBrowsingStandard.click();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noValueChangeSafeBrowsingReportingInDisabled', async function() {
    page.$.safeBrowsingStandard.click();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$.safeBrowsingDisabled.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    await clickConfirmOnDisableSafebrowsingDialog(page);

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noValueChangePasswordLeakSwitchToEnhanced', async () => {
    page.$.safeBrowsingStandard.click();
    await microtasksFinished();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$.safeBrowsingEnhanced.click();
    await eventToPromise('selected-changed', page.$.safeBrowsingRadioGroup);

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('noValuePasswordLeakSwitchToDisabled', async function() {
    page.$.safeBrowsingStandard.click();
    await microtasksFinished();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$.safeBrowsingDisabled.click();
    await microtasksFinished();

    // Previously selected option must remain opened.
    assertTrue(page.$.safeBrowsingStandard.expanded);

    await clickConfirmOnDisableSafebrowsingDialog(page);

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

  test('StandardProtectionDropdown', async () => {
    loadTimeData.overrideValues({enableHashPrefixRealTimeLookups: false});
    resetRouterForTesting();

    await resetPage();
    const standardProtection = page.$.safeBrowsingStandard;
    const spSubLabel = loadTimeData.getString('safeBrowsingStandardDesc');
    assertEquals(spSubLabel, standardProtection.subLabel);

    const passwordsLeakToggle = page.$.passwordsLeakToggle;
    const passwordLeakLabel =
        loadTimeData.getString('passwordsLeakDetectionLabel');
    assertEquals(passwordLeakLabel, passwordsLeakToggle.label);

    const passwordLeakSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription');
    assertEquals(passwordLeakSubLabel, passwordsLeakToggle.subLabel);
  });

  test('EnhancedProtectionText', async () => {
    const enhancedProtection = page.$.safeBrowsingEnhanced;
    const epSubLabel = loadTimeData.getString('safeBrowsingEnhancedDesc');
    assertEquals(epSubLabel, enhancedProtection.subLabel);

    const noProtection = page.$.safeBrowsingDisabled;
    const npSubLabel = loadTimeData.getString('safeBrowsingNoneDesc');
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
    loadTimeData.overrideValues({enableHashPrefixRealTimeLookups: true});
    resetRouterForTesting();

    await resetPage();
    const standardProtection = page.$.safeBrowsingStandard;
    const subLabel = loadTimeData.getString('safeBrowsingStandardDescProxy');
    assertEquals(subLabel, standardProtection.subLabel);
  });
  // </if>

  // <if expr="not _google_chrome">
  test('StandardProtectionDropdownNoProxyStringForChromium', function() {
    // If this test fails, it may be because hash-prefix real-time lookups have
    // been enabled for Chromium. The settings strings only currently support
    // Chrome, so this must be addressed to support Chromium as well.
    const standardProtection = page.$.safeBrowsingStandard;
    const subLabel = loadTimeData.getString('safeBrowsingStandardDesc');
    assertEquals(subLabel, standardProtection.subLabel);
  });
  // </if>

  test(
      'SafeBrowsingReportingToggleNotVisibleWhenExtendedReportingDeprecatedAndHprtSampled',
      async function() {
        // The safeBrowsingReportingToggle should not be visible if the extended
        // reporting deprecation flag is enabled and HPRT sampled lookup flag is
        // enabled.
        loadTimeData.overrideValues({
          extendedReportingRemovePrefDependency: true,
          hashPrefixRealTimeLookupsSamplePing: true,
        });
        resetRouterForTesting();

        await resetPage();
        page.$.safeBrowsingStandard.click();

        await microtasksFinished();
        assertTrue(page.$.safeBrowsingStandard.expanded);

        assertFalse(isChildVisible(page, '#safeBrowsingReportingToggle'));
      });

  test(
      'SafeBrowsingReportingToggleVisibleWhenExtendedReportingDeprecatedAndHprtNotSampled',
      async function() {
        // The safeBrowsingReportingToggle should be visible if the extended
        // reporting deprecation flag is enabled and HPRT sampled lookup flag is
        // disabled.
        loadTimeData.overrideValues({
          extendedReportingRemovePrefDependency: true,
          hashPrefixRealTimeLookupsSamplePing: false,
        });
        resetRouterForTesting();

        await resetPage();
        page.$.safeBrowsingStandard.click();

        await microtasksFinished();
        assertTrue(page.$.safeBrowsingStandard.expanded);

        assertTrue(isChildVisible(page, '#safeBrowsingReportingToggle'));
      });

  test(
      'SafeBrowsingReportingToggleVisibleWhenExtendedReportingNotDeprecatedAndHprtSampled',
      async function() {
        // The safeBrowsingReportingToggle should be visible if the extended
        // reporting deprecation flag is disabled and HPRT sampled lookup flag
        // is enabled.
        loadTimeData.overrideValues({
          extendedReportingRemovePrefDependency: false,
          hashPrefixRealTimeLookupsSamplePing: true,
        });
        resetRouterForTesting();

        await resetPage();
        page.$.safeBrowsingStandard.click();

        await microtasksFinished();
        assertTrue(page.$.safeBrowsingStandard.expanded);

        assertTrue(isChildVisible(page, '#safeBrowsingReportingToggle'));
      });
});

async function clickCancelOnDisableSafebrowsingDialog(
    page: SettingsSecurityPageElement) {
  const confirmationDialog =
      page.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
  assertTrue(!!confirmationDialog);
  const closePromise = eventToPromise('close', confirmationDialog);
  confirmationDialog.$.cancel.click();
  flush();
  await closePromise;
  await microtasksFinished();
}

async function clickConfirmOnDisableSafebrowsingDialog(
    page: SettingsSecurityPageElement) {
  const confirmationDialog =
      page.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
  assertTrue(!!confirmationDialog);
  const closePromise = eventToPromise('close', confirmationDialog);
  confirmationDialog.$.confirm.click();
  flush();
  await closePromise;
  await microtasksFinished();
}
