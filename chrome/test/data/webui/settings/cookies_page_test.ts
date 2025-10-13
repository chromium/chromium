// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsCollapseRadioButtonElement, SettingsRadioGroupElement, SettingsCookiesPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSettingsTypes, SITE_EXCEPTION_WILDCARD, SiteSettingsBrowserProxyImpl,ThirdPartyCookieBlockingSetting} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, PrivacyElementInteractions, resetRouterForTesting, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

suite('CookiesPageTest', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  function thirdPartyCookieBlockingSettingGroup(): SettingsRadioGroupElement {
    const group = page.shadowRoot!.querySelector<SettingsRadioGroupElement>(
        '#thirdPartyCookieBlockingSettingGroup');
    assertTrue(!!group);
    return group;
  }

  function blockAll3pc(): SettingsCollapseRadioButtonElement {
    const blockAll3pc =
        page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#blockAll3pc');
    assertTrue(!!blockAll3pc);
    return blockAll3pc;
  }

  function block3pcIncognito(): SettingsCollapseRadioButtonElement {
    const block3pcIncognito =
        page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#block3pcIncognito');
    assertTrue(!!block3pcIncognito);
    return block3pcIncognito;
  }

  function createPage() {
    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;

    // Enable one of the PS APIs.
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    page.set(
        'prefs.generated.third_party_cookie_blocking_setting.value',
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    document.body.appendChild(page);
    flush();
  }

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: false,
    });
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    createPage();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('SubpageTitle', function() {
    assertEquals(
        page.i18n('thirdPartyCookiesPageTitle'),
        page.shadowRoot!.querySelector('settings-subpage')!.getAttribute(
            'page-title'));
  });

  test('ElementVisibility', async function() {
    await flushTasks();
    assertTrue(isChildVisible(page, '#explanationText'));
    assertTrue(isChildVisible(page, '#generalControls'));
    assertTrue(isChildVisible(page, '#additionalProtections'));
    assertTrue(isChildVisible(page, '#exceptionHeader3pcd'));
    assertTrue(isChildVisible(page, '#allow3pcExceptionsList'));
    // Controls
    assertTrue(isChildVisible(page, '#doNotTrack'));
    assertTrue(isChildVisible(page, '#blockAll3pc'));
    assertTrue(isChildVisible(page, '#block3pcIncognito'));
    // Mode B only
    assertFalse(isChildVisible(page, '#blockThirdPartyToggle'));
    assertFalse(isChildVisible(page, '#allowThirdParty'));
  });


  test('thirdPartyCookiesRadioClicksRecorded', async function() {
    blockAll3pc().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    let result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK, result);
    assertEquals(
        'Settings.ThirdPartyCookies.Block',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.reset();

    block3pcIncognito().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(
        PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK_IN_INCOGNITO,
        result);
    assertEquals(
        'Settings.ThirdPartyCookies.Allow',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.reset();
  });


  test('privacySandboxToast', async function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    resetRouterForTesting();
    await createPage();
    assertFalse(page.$.toast.open);

    // Disabling 3P cookies should display the privacy sandbox toast.
    page.set(
        'prefs.generated.third_party_cookie_blocking_setting.value',
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    blockAll3pc().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    assertTrue(page.$.toast.open);

    // Re-enabling 3P cookies should not display the toast.
    block3pcIncognito().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    assertFalse(page.$.toast.open);

    // The toast should not be displayed if the user has any privacy sandbox
    // APIs disabled.
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', false);
    blockAll3pc().click();
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    assertFalse(page.$.toast.open);

    // Reset the state to show the toast.
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    page.set(
        'prefs.generated.third_party_cookie_blocking_setting.value',
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    blockAll3pc().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    assertTrue(page.$.toast.open);

    // Reselecting a non-3P cookie blocking setting should hide the toast.
    block3pcIncognito().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    assertFalse(page.$.toast.open);
  });

  test('privacySandboxToast_restrictedSandbox', async function() {
    // No toast should be shown if the privacy sandbox is restricted.
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
    });
    resetRouterForTesting();
    await createPage();

    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    blockAll3pc().click();
    assertEquals(
        'Settings.ThirdPartyCookies.Block',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertFalse(page.$.toast.open);
  });

  test('disabledRWSToggle', async () => {
    // Verify the RWS toggle is enabled iff the user has selected block 3PCs.
    const relatedWebsiteSetsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#relatedWebsiteSetsToggle3pcSetting')!;
    blockAll3pc().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY,
        page.prefs.generated.third_party_cookie_blocking_setting.value);
    assertFalse(
        relatedWebsiteSetsToggle.disabled, 'expect toggle to be enabled');

    block3pcIncognito().click();
    await eventToPromise('change', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY,
        page.prefs.generated.third_party_cookie_blocking_setting.value);
    assertTrue(
        relatedWebsiteSetsToggle.disabled, 'expect toggle to be disabled');
  });
});

suite('ExceptionsList', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

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
