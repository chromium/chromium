// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsCollapseRadioButtonElement, SettingsRadioGroupElement, SettingsCookiesPageElement} from 'chrome://settings/lazy_load.js';
import {CookieControlsMode, ContentSettingsTypes, SITE_EXCEPTION_WILDCARD, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyElementInteractions, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

suite('CookiesPageTest', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  function primarySettingGroup(): SettingsRadioGroupElement {
    const group = page.shadowRoot!.querySelector<SettingsRadioGroupElement>(
        '#primarySettingGroup');
    assertTrue(!!group);
    return group;
  }

  function blockThirdParty(): SettingsCollapseRadioButtonElement {
    return page.shadowRoot!.querySelector('#blockThirdParty')!;
  }

  function blockThirdPartyIncognito(): SettingsCollapseRadioButtonElement {
    return page.shadowRoot!.querySelector('#blockThirdPartyIncognito')!;
  }

  function allowThirdParty(): SettingsCollapseRadioButtonElement {
    return page.shadowRoot!.querySelector('#allowThirdParty')!;
  }

  suiteSetup(function() {
    // This test is for the pre-3PCD cookies page.
    loadTimeData.overrideValues({is3pcdCookieSettingsRedesignEnabled: false});
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;

    // Enable one of the PS APIs.
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    page.set(
        'prefs.profile.cookie_controls_mode.value', CookieControlsMode.OFF);
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('ElementVisibility', async function() {
    await flushTasks();
    // Headers
    assertTrue(isChildVisible(page, '#explanationText'));
    assertTrue(isChildVisible(page, '#generalControls'));
    assertTrue(isChildVisible(page, '#additionalProtections'));
    assertTrue(isChildVisible(page, '#exceptionHeader3pcd'));
    assertTrue(isChildVisible(page, '#allow3pcExceptionsList'));
    // Settings
    assertTrue(isChildVisible(page, '#doNotTrack'));
    assertTrue(isChildVisible(page, '#allowThirdParty'));
    assertTrue(isChildVisible(page, '#blockThirdParty'));
    assertTrue(isChildVisible(page, '#blockThirdPartyIncognito'));
    // By default these toggles should be hidden.
    assertFalse(isChildVisible(page, '#blockThirdPartyToggle'));
    assertFalse(isChildVisible(page, '#ipProtectionToggle'));
    assertFalse(isChildVisible(page, '#fingerprintingProtectionToggle'));
  });

  test('ThirdPartyCookiesRadioClicksRecorded', async function() {
    blockThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    let result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK, result);
    testMetricsBrowserProxy.reset();

    blockThirdPartyIncognito().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.INCOGNITO_ONLY);
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(
        PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK_IN_INCOGNITO,
        result);
    testMetricsBrowserProxy.reset();

    allowThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.OFF);
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.THIRD_PARTY_COOKIES_ALLOW, result);
    testMetricsBrowserProxy.reset();
  });

  test('privacySandboxToast', async function() {
    assertFalse(page.$.toast.open);

    // Disabling third-party cookies should display the privacy sandbox toast.
    blockThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    // TODO(crbug.com/40244046): Check historgrams.
    assertTrue(page.$.toast.open);

    // Clicking the toast link should be recorded in UMA and should dismiss
    // the toast.
    page.$.toast.querySelector('cr-button')!.click();
    // TODO(crbug.com/40244046): Check historgrams.
    assertFalse(page.$.toast.open);

    // Renabling 3P cookies for regular sessions should not display the toast.
    blockThirdPartyIncognito().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.INCOGNITO_ONLY);
    assertFalse(page.$.toast.open);

    // The toast should not be displayed if the user has all privacy sandbox
    // APIs disabled.
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', false);
    blockThirdParty().click();
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    assertFalse(page.$.toast.open);

    // Reset the state to show the toast.
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.INCOGNITO_ONLY);
    blockThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    // TODO(crbug.com/40244046): Check historgrams.
    assertTrue(page.$.toast.open);

    // Reselecting a non-3P cookie blocking setting should hide the toast.
    allowThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.OFF);
    assertFalse(page.$.toast.open);

    // Navigating away from the page should hide the toast, even if navigated
    // back to.
    blockThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    assertTrue(page.$.toast.open);
    Router.getInstance().navigateTo(routes.BASIC);
    Router.getInstance().navigateTo(routes.COOKIES);
    await flushTasks();
    assertFalse(page.$.toast.open);
  });

  test('privacySandboxToast_restrictedSandbox', async function() {
    // No toast should be shown if the privacy sandbox is restricted
    loadTimeData.overrideValues({isPrivacySandboxRestricted: true});
    resetRouterForTesting();

    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    blockThirdParty().click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertFalse(page.$.toast.open);
  });

  test('disabledRWSToggle', async () => {
    // Confirm that when the user has not selected the block 3PC setting, the
    // RWS toggle is disabled.
    const relatedWebsiteSetsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#relatedWebsiteSetsToggle')!;
    blockThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    assertEquals(
        CookieControlsMode.BLOCK_THIRD_PARTY,
        page.prefs.profile.cookie_controls_mode.value);
    assertFalse(
        relatedWebsiteSetsToggle.disabled, 'expect toggle to be enabled');

    allowThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    assertEquals(
        CookieControlsMode.OFF, page.prefs.profile.cookie_controls_mode.value);
    assertTrue(
        relatedWebsiteSetsToggle.disabled, 'expect toggle to be disabled');

    blockThirdPartyIncognito().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    assertEquals(
        CookieControlsMode.INCOGNITO_ONLY,
        page.prefs.profile.cookie_controls_mode.value);
    assertTrue(
        relatedWebsiteSetsToggle.disabled, 'expect toggle to be disabled');
  });

  test('blockThirdPartyIncognitoSecondBulletPointText', function() {
    // Confirm the correct string is set.
    const cookiesPageBlockThirdPartyIncognitoBulTwoLabel =
        page.shadowRoot!
            .querySelector<HTMLElement>(
                '#blockThirdPartyIncognitoBulTwo')!.innerText.trim();
    assertEquals(
        loadTimeData.getString('cookiePageBlockThirdIncognitoBulTwoRws'),
        cookiesPageBlockThirdPartyIncognitoBulTwoLabel);
  });
});

suite('ExceptionsList', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    flush();
  });

  test('ExceptionsSearch', async function() {
    await siteSettingsBrowserProxy.whenCalled('getExceptionList');
    siteSettingsBrowserProxy.resetResolver('getExceptionList');

    const exceptionPrefs = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.COOKIES,
          [
            createRawSiteException(SITE_EXCEPTION_WILDCARD, {
              embeddingOrigin: 'foo-allow.com',
            }),
          ]),
    ]);
    page.searchTerm = 'foo';
    siteSettingsBrowserProxy.setPrefs(exceptionPrefs);
    await siteSettingsBrowserProxy.whenCalled('getExceptionList');
    flush();

    const exceptionList = page.shadowRoot!.querySelector('site-list');
    assertTrue(!!exceptionList);
    assertTrue(isChildVisible(exceptionList, 'site-list-entry'));

    page.searchTerm = 'unrelated.com';
    flush();

    assertFalse(isChildVisible(exceptionList, 'site-list-entry'));
  });

  test('ExceptionListHasCorrectCookieExceptionType', function() {
    const exceptionList = page.shadowRoot!.querySelector('site-list');
    assertTrue(!!exceptionList);
    assertEquals(
        'third-party', exceptionList.getAttribute('cookies-exception-type'));
  });
});

// TODO(crbug.com/40233724): Remove after crbug/1349370 is launched.
suite('FirstPartySetsUIDisabled', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      firstPartySetsUIEnabled: false,
      // FirstPartySetsUI does not exist in 3PCD.
      is3pcdCookieSettingsRedesignEnabled: false,
    });
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('blockThirdPartyIncognitoSecondBulletPointText', function() {
    // Confirm the correct string is set.
    const cookiesPageBlockThirdPartyIncognitoBulTwoLabel =
        page.shadowRoot!
            .querySelector<HTMLElement>(
                '#blockThirdPartyIncognitoBulTwo')!.innerText.trim();
    assertEquals(
        loadTimeData.getString('thirdPartyCookiesPageBlockIncognitoBulTwo'),
        cookiesPageBlockThirdPartyIncognitoBulTwoLabel);
  });
});

suite('TrackingProtectionSettings', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({is3pcdCookieSettingsRedesignEnabled: true});
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    flush();
  });

  test('CheckVisibility', function() {
    // Page description
    assertTrue(isChildVisible(page, '#default'));
    assertEquals(
        page.shadowRoot!.querySelector<HTMLAnchorElement>(
                            'a[href]')!.getAttribute('aria-description'),
        page.i18n('opensInNewTab'));

    // Additional toggles
    assertTrue(isChildVisible(page, '#blockThirdPartyToggle'));
    assertTrue(isChildVisible(page, '#doNotTrack'));

    // Site Exception list
    assertFalse(isChildVisible(page, '#exceptionHeader'));
    assertFalse(isChildVisible(page, '#exceptionHeaderSubLabel'));
    assertTrue(isChildVisible(page, '#exceptionHeader3pcd'));
    assertTrue(isChildVisible(page, '#allow3pcExceptionsList'));
  });

  test('BlockAll3pcToggle', async function() {
    page.set(
        'prefs.tracking_protection.block_all_3pc_toggle_enabled.value', false);
    const blockThirdPartyCookiesToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#blockThirdPartyToggle')!;
    assertTrue(!!blockThirdPartyCookiesToggle);

    blockThirdPartyCookiesToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(
        PrivacyElementInteractions.BLOCK_ALL_THIRD_PARTY_COOKIES, result);
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    assertEquals(
        page.getPref('tracking_protection.block_all_3pc_toggle_enabled.value'),
        true);
  });
});

suite('ActSettings', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isIpProtectionUxEnabled: true,
      isFingerprintingProtectionUxEnabled: true,
    });
    resetRouterForTesting();

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    flush();
  });

  test('CheckVisibility', function() {
    // Settings are visible
    assertTrue(isChildVisible(page, '#ipProtectionToggle'));
    assertTrue(isChildVisible(page, '#fingerprintingProtectionToggle'));
  });

  test('ToggleIpProtection', async function() {
    page.set('prefs.tracking_protection.ip_protection_enabled.value', false);
    const ipProtectionToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#ipProtectionToggle')!;
    assertTrue(!!ipProtectionToggle);

    ipProtectionToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.IP_PROTECTION, result);
    assertEquals(
        page.getPref('tracking_protection.ip_protection_enabled.value'), true);
  });

  test('ToggleFingerprintingProtection', async function() {
    page.set(
        'prefs.tracking_protection.fingerprinting_protection_enabled.value',
        false);
    const fingerprintingProtectionToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#fingerprintingProtectionToggle')!;
    assertTrue(!!fingerprintingProtectionToggle);

    fingerprintingProtectionToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.FINGERPRINTING_PROTECTION, result);
    assertEquals(
        page.getPref(
            'tracking_protection.fingerprinting_protection_enabled.value'),
        true);
  });
});
