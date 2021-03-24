// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isLacros, isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SafeBrowsingSetting} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, PrivacyPageBrowserProxyImpl, Router, routes, SafeBrowsingInteractions, SecureDnsMode} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {flushTasks, isChildVisible} from '../test_util.m.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

suite('CrSettingsSecurityPageTest', function() {
  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {!TestPrivacyPageBrowserProxy} */
  let testPrivacyBrowserProxy;

  /** @type {!SettingsSecurityPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: true,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    testPrivacyBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.instance_ = testPrivacyBrowserProxy;
    document.body.innerHTML = '';
    page = /** @type {!SettingsSecurityPageElement} */ (
        document.createElement('settings-security-page'));
    page.prefs = {
      profile: {password_manager_leak_detection: {value: false}},
      safebrowsing: {
        scout_reporting_enabled: {value: true},
      },
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
        password_leak_detection: {value: false},
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(page);
    page.$$('#safeBrowsingEnhanced').updateCollapsed();
    page.$$('#safeBrowsingStandard').updateCollapsed();
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  if (isMac || isWindows) {
    test('NativeCertificateManager', function() {
      page.$$('#manageCertificates').click();
      return testPrivacyBrowserProxy.whenCalled('showManageSSLCertificates');
    });
  }

  // Initially specified pref option should be expanded
  test('SafeBrowsingRadio_InitialPrefOptionIsExpanded', function() {
    assertFalse(page.$$('#safeBrowsingEnhanced').expanded);
    assertTrue(page.$$('#safeBrowsingStandard').expanded);
  });

  // TODO(crbug.com/1148302): This class directly calls
  // `GetNSSCertDatabaseForProfile()` that causes crash at the moment and is
  // never called from Lacros-Chrome. This should be revisited when there is a
  // solution for the client certificates settings page on Lacros-Chrome.
  if (!isLacros) {
    test('LogManageCerfificatesClick', async function() {
      page.$$('#manageCertificates').click();
      const result = await testMetricsBrowserProxy.whenCalled(
          'recordSettingsPageHistogram');
      assertEquals(PrivacyElementInteractions.MANAGE_CERTIFICATES, result);
    });
  }

  test('ManageSecurityKeysSubpageVisible', function() {
    assertTrue(isChildVisible(page, '#security-keys-subpage-trigger'));
  });

  test('PasswordsLeakDetectionSubLabel', function() {
    const toggle = page.$$('#passwordsLeakToggle');
    const defaultSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription');
    const activeWhenSignedInSubLabel =
        loadTimeData.getString('passwordsLeakDetectionGeneralDescription') +
        ' ' +
        loadTimeData.getString(
            'passwordsLeakDetectionSignedOutEnabledDescription');
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', true);
    flush();
    assertEquals(activeWhenSignedInSubLabel, toggle.subLabel);

    page.set('prefs.generated.password_leak_detection.value', true);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);

    page.set('prefs.profile.password_manager_leak_detection.value', false);
    flush();
    assertEquals(defaultSubLabel, toggle.subLabel);
  });

  test('LogSafeBrowsingExtendedToggle', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();

    page.$$('#safeBrowsingReportingToggle').click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.IMPROVE_SECURITY, result);
  });

  test('safeBrowsingReportingToggle', function() {
    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);

    const safeBrowsingReportingToggle = page.$$('#safeBrowsingReportingToggle');
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);

    // This could also be set to disabled, anything other than standard.
    page.$$('#safeBrowsingEnhanced').click();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();
    assertTrue(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
    assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);

    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();
    assertFalse(safeBrowsingReportingToggle.disabled);
    assertTrue(safeBrowsingReportingToggle.checked);
  });

  test(
      'SafeBrowsingRadio_ManuallyExpandedRemainExpandedOnRepeatSelection',
      function() {
        page.$$('#safeBrowsingStandard').click();
        flush();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);
        assertTrue(page.$$('#safeBrowsingStandard').expanded);
        assertFalse(page.$$('#safeBrowsingEnhanced').expanded);

        // Expanding another radio button should not collapse already expanded
        // option.
        page.$$('#safeBrowsingEnhanced').$$('cr-expand-button').click();
        flush();
        assertTrue(page.$$('#safeBrowsingStandard').expanded);
        assertTrue(page.$$('#safeBrowsingEnhanced').expanded);

        // Clicking on already selected button should not collapse manually
        // expanded option.
        page.$$('#safeBrowsingStandard').click();
        flush();
        assertTrue(page.$$('#safeBrowsingStandard').expanded);
        assertTrue(page.$$('#safeBrowsingEnhanced').expanded);
      });

  test(
      'SafeBrowsingRadio_ManuallyExpandedRemainExpandedOnSelectedChanged',
      async function() {
        page.$$('#safeBrowsingStandard').click();
        flush();
        assertEquals(
            SafeBrowsingSetting.STANDARD,
            page.prefs.generated.safe_browsing.value);

        page.$$('#safeBrowsingEnhanced').$$('cr-expand-button').click();
        flush();
        assertTrue(page.$$('#safeBrowsingStandard').expanded);
        assertTrue(page.$$('#safeBrowsingEnhanced').expanded);

        page.$$('#safeBrowsingDisabled').click();
        flush();

        // Previously selected option must remain opened.
        assertTrue(page.$$('#safeBrowsingStandard').expanded);
        assertTrue(page.$$('#safeBrowsingEnhanced').expanded);

        page.$$('settings-disable-safebrowsing-dialog')
            .$$('.action-button')
            .click();
        flush();

        // Wait for onDisableSafebrowsingDialogClose_ to finish.
        await flushTasks();

        // The deselected option should become collapsed.
        assertFalse(page.$$('#safeBrowsingStandard').expanded);
        assertTrue(page.$$('#safeBrowsingEnhanced').expanded);
      });

  test('DisableSafebrowsingDialog_Confirm', async function() {
    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();

    page.$$('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.$$('#safeBrowsingStandard').expanded);

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(null, page.$$('settings-disable-safebrowsing-dialog'));

    assertFalse(page.$$('#safeBrowsingEnhanced').checked);
    assertFalse(page.$$('#safeBrowsingStandard').checked);
    assertTrue(page.$$('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.DISABLED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromEnhanced', async function() {
    page.$$('#safeBrowsingEnhanced').click();
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
    flush();

    page.$$('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.$$('#safeBrowsingEnhanced').expanded);

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.cancel-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(null, page.$$('settings-disable-safebrowsing-dialog'));

    assertTrue(page.$$('#safeBrowsingEnhanced').checked);
    assertFalse(page.$$('#safeBrowsingStandard').checked);
    assertFalse(page.$$('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.ENHANCED, page.prefs.generated.safe_browsing.value);
  });

  test('DisableSafebrowsingDialog_CancelFromStandard', async function() {
    page.$$('#safeBrowsingStandard').click();
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    flush();

    page.$$('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.$$('#safeBrowsingStandard').expanded);

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.cancel-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertEquals(null, page.$$('settings-disable-safebrowsing-dialog'));

    assertFalse(page.$$('#safeBrowsingEnhanced').checked);
    assertTrue(page.$$('#safeBrowsingStandard').checked);
    assertFalse(page.$$('#safeBrowsingDisabled').checked);
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
  });

  test('noControlSafeBrowsingReportingInEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    flush();

    assertFalse(page.$$('#safeBrowsingReportingToggle').disabled);
    page.$$('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(page.$$('#safeBrowsingReportingToggle').disabled);
  });

  test('noValueChangeSafeBrowsingReportingInEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$$('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noControlSafeBrowsingReportingInDisabled', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();

    assertFalse(page.$$('#safeBrowsingReportingToggle').disabled);
    page.$$('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.$$('#safeBrowsingStandard').expanded);

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(page.$$('#safeBrowsingReportingToggle').disabled);
  });

  test('noValueChangeSafeBrowsingReportingInDisabled', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.safebrowsing.scout_reporting_enabled.value;

    page.$$('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.$$('#safeBrowsingStandard').expanded);

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.safebrowsing.scout_reporting_enabled.value === previous);
  });

  test('noValueChangePasswordLeakSwitchToEnhanced', function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$$('#safeBrowsingEnhanced').click();
    flush();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('noValuePasswordLeakSwitchToDisabled', async function() {
    page.$$('#safeBrowsingStandard').click();
    flush();
    const previous = page.prefs.profile.password_manager_leak_detection.value;

    page.$$('#safeBrowsingDisabled').click();
    flush();

    // Previously selected option must remain opened.
    assertTrue(page.$$('#safeBrowsingStandard').expanded);

    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();

    // Wait for onDisableSafebrowsingDialogClose_ to finish.
    await flushTasks();

    assertTrue(
        page.prefs.profile.password_manager_leak_detection.value === previous);
  });

  test('safeBrowsingUserActionRecorded', async function() {
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    page.$$('#safeBrowsingStandard').click();
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
    page.$$('#safeBrowsingEnhanced').click();
    flush();
    const [enhancedClickedResult, enhancedClickedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
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
    page.$$('#safeBrowsingEnhanced').$$('cr-expand-button').click();
    flush();
    const [enhancedExpandedResult, enhancedExpandedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
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
    page.$$('#safeBrowsingStandard').$$('cr-expand-button').click();
    flush();
    const [standardExpandedResult, standardExpandedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
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
    page.$$('#safeBrowsingDisabled').click();
    flush();
    const [disableClickedResult, disableClickedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
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
    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.cancel-button')
        .click();
    flush();
    const [disableDeniedResult, disableDeniedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
    ]);
    assertEquals(
        SafeBrowsingInteractions
            .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED,
        disableDeniedResult);
    assertEquals(
        'SafeBrowsing.Settings.DisableSafeBrowsingDialogDenied',
        disableDeniedAction);

    await flushTasks();

    page.$$('#safeBrowsingDisabled').click();
    flush();
    testMetricsBrowserProxy.resetResolver(
        'recordSafeBrowsingInteractionHistogram');
    testMetricsBrowserProxy.resetResolver('recordAction');
    page.$$('settings-disable-safebrowsing-dialog')
        .$$('.action-button')
        .click();
    flush();
    const [disableConfirmedResult, disableConfirmedAction] = await Promise.all([
      testMetricsBrowserProxy.whenCalled(
          'recordSafeBrowsingInteractionHistogram'),
      testMetricsBrowserProxy.whenCalled('recordAction')
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
        routes.SECURITY, /* dynamicParams= */ null,
        /* removeSearch= */ true);
    assertEquals(
        SafeBrowsingInteractions.SAFE_BROWSING_SHOWED,
        await testMetricsBrowserProxy.whenCalled(
            'recordSafeBrowsingInteractionHistogram'));
  });

  test('enhancedProtectionAutoExpanded', function() {
    // Standard protection should be pre-expanded if there is no param.
    Router.getInstance().navigateTo(routes.SECURITY);
    assertFalse(page.$$('#safeBrowsingEnhanced').expanded);
    assertTrue(page.$$('#safeBrowsingStandard').expanded);
    // Enhanced protection should be pre-expanded if the param is set to
    // enhanced.
    Router.getInstance().navigateTo(
        routes.SECURITY,
        /* dynamicParams= */ new URLSearchParams('q=enhanced'));
    assertEquals(
        SafeBrowsingSetting.STANDARD, page.prefs.generated.safe_browsing.value);
    assertTrue(page.$$('#safeBrowsingEnhanced').expanded);
    assertFalse(page.$$('#safeBrowsingStandard').expanded);
  });
});


suite('CrSettingsSecurityPageTest_FlagsDisabled', function() {
  /** @type {!SettingsSecurityPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSecurityKeysSubpage: false,
    });
  });

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsSecurityPageElement} */ (
        document.createElement('settings-security-page'));
    page.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing: {
        scout_reporting_enabled: {value: true},
      },
      generated: {
        safe_browsing: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SafeBrowsingSetting.STANDARD,
        },
        password_leak_detection: {value: true, userControlDisabled: false},
      },
      dns_over_https:
          {mode: {value: SecureDnsMode.AUTOMATIC}, templates: {value: ''}},
    };
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('ManageSecurityKeysSubpageHidden', function() {
    assertFalse(isChildVisible(page, '#security-keys-subpage-trigger'));
  });
});
