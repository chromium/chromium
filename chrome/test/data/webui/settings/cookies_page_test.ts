// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsCollapseRadioButtonElement, SettingsRadioGroupElement, SettingsCookiesPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSettingsTypes, SITE_EXCEPTION_WILDCARD, SiteSettingsBrowserProxyImpl,ThirdPartyCookieBlockingSetting} from 'chrome://settings/lazy_load.js';
import type {ControlledRadioButtonElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {loadTimeData, MetricsBrowserProxyImpl, PrefsBrowserProxy, PrefService, PrivacyElementInteractions, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'enable_do_not_track',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'generated.third_party_cookie_blocking_setting',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY,
    },
    {
      key: 'universal_optout.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
  ];
}

suite('CookiesPageTest', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsCookiesPageElement;
  let prefService: PrefService;

  function thirdPartyCookieBlockingSettingGroup(): SettingsRadioGroupElement {
    const group = page.shadowRoot.querySelector<SettingsRadioGroupElement>(
        '#thirdPartyCookieBlockingSettingGroup');
    assertTrue(!!group);
    return group;
  }

  function blockAll3pc(): SettingsCollapseRadioButtonElement {
    const blockAll3pc =
        page.shadowRoot.querySelector<SettingsCollapseRadioButtonElement>(
            '#blockAll3pc');
    assertTrue(!!blockAll3pc);
    return blockAll3pc;
  }

  function block3pcIncognito(): SettingsCollapseRadioButtonElement {
    const block3pcIncognito =
        page.shadowRoot.querySelector<SettingsCollapseRadioButtonElement>(
            '#block3pcIncognito');
    assertTrue(!!block3pcIncognito);
    return block3pcIncognito;
  }

  function createPage() {
    page = document.createElement('settings-cookies-page');
    document.body.appendChild(page);
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({settingsRefresh2026: ''});
  });

  setup(async function() {
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    createPage();
    await microtasksFinished();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('SubpageTitle', function() {
    assertEquals(
        page.i18n('thirdPartyCookiesPageTitle'),
        page.shadowRoot.querySelector('settings-subpage')!.pageTitle);
  });

  test('ElementVisibility', async function() {
    await flushTasks();
    assertTrue(isChildVisible(page, '#explanationText'));
    assertTrue(isChildVisible(page, '#generalControls'));
    assertTrue(isChildVisible(page, '#additionalProtections'));
    assertFalse(isChildVisible(page, '#cookiesHeader'));
    assertFalse(isChildVisible(page, '#siteRequestsHeader'));
    assertTrue(isChildVisible(page, '#exceptionHeader'));
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
        prefService.getPref('generated.third_party_cookie_blocking_setting')
            .value,
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
        prefService.getPref('generated.third_party_cookie_blocking_setting')
            .value,
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
});

suite('UniversalOptOut', function() {
  let page: SettingsCookiesPageElement;
  let prefService: PrefService;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  async function createPage(showSettings: boolean) {
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({showUniversalOptOutSettings: showSettings});

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    page = document.createElement('settings-cookies-page');

    Router.getInstance().navigateTo(routes.COOKIES);
    document.body.appendChild(page);
    await microtasksFinished();
  }

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('UniversalOptOutEnabled', async function() {
    await createPage(true);
    const subpage = page.shadowRoot.querySelector('settings-subpage');
    assertTrue(!!subpage);
    assertEquals(
        page.i18n('thirdPartyCookiesAndSiteDataPageTitle'), subpage.pageTitle);
    assertTrue(isChildVisible(page, '#cookiesHeader'));
    assertTrue(isChildVisible(page, '#siteRequestsHeader'));
    assertFalse(isChildVisible(page, '#additionalProtections'));
    assertTrue(isChildVisible(page, '#universalOptOutToggle'));

    const [histogramName, visible] =
        await testMetricsBrowserProxy.whenCalled('recordBooleanHistogram');
    assertEquals('Privacy.UniversalOptOut.SettingsVisibility', histogramName);
    assertTrue(visible);

    const toggle = page.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#universalOptOutToggle');
    assertTrue(!!toggle);
    assertEquals(page.i18n('universalOptOutLearnMoreURL'), toggle.learnMoreUrl);

    assertFalse(toggle.checked);
    assertFalse(prefService.getPref<boolean>('universal_optout.enabled').value);

    toggle.click();
    await microtasksFinished();
    assertTrue(toggle.checked);
    assertTrue(prefService.getPref<boolean>('universal_optout.enabled').value);
    assertEquals(
        'Privacy.UniversalOptOut.SettingsToggleOn',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.reset();

    toggle.click();
    await microtasksFinished();
    assertFalse(toggle.checked);
    assertFalse(prefService.getPref<boolean>('universal_optout.enabled').value);
    assertEquals(
        'Privacy.UniversalOptOut.SettingsToggleOff',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.reset();
  });

  test('UniversalOptOutDisabled', async function() {
    await createPage(false);
    const subpage = page.shadowRoot.querySelector('settings-subpage');
    assertTrue(!!subpage);
    assertEquals(page.i18n('thirdPartyCookiesPageTitle'), subpage.pageTitle);
    assertFalse(isChildVisible(page, '#cookiesHeader'));
    assertFalse(isChildVisible(page, '#siteRequestsHeader'));
    assertTrue(isChildVisible(page, '#additionalProtections'));
    assertFalse(isChildVisible(page, '#universalOptOutToggle'));

    const [histogramName, visible] =
        await testMetricsBrowserProxy.whenCalled('recordBooleanHistogram');
    assertEquals('Privacy.UniversalOptOut.SettingsVisibility', histogramName);
    assertFalse(visible);
  });
});

suite('ExceptionsList', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let page: SettingsCookiesPageElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    page = document.createElement('settings-cookies-page');
    document.body.appendChild(page);
    await microtasksFinished();
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
    await microtasksFinished();
    flush();

    const exceptionList = page.shadowRoot.querySelector('site-list');
    assertTrue(!!exceptionList);
    assertTrue(isChildVisible(exceptionList, 'site-list-entry'));

    page.searchTerm = 'unrelated.com';
    await microtasksFinished();
    flush();

    assertFalse(isChildVisible(exceptionList, 'site-list-entry'));
  });

  test('ExceptionListHasCorrectCookieExceptionType', function() {
    const exceptionList = page.shadowRoot.querySelector('site-list');
    assertTrue(!!exceptionList);
    assertEquals(
        'third-party', exceptionList.getAttribute('cookies-exception-type'));
  });
});

suite('CookiesPageSettingsRefresh2026Test', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsCookiesPageElement;
  let prefService: PrefService;

  function thirdPartyCookieBlockingSettingGroup(): SettingsRadioGroupElement {
    const group = page.shadowRoot.querySelector<SettingsRadioGroupElement>(
        '#thirdPartyCookieBlockingSettingGroup');
    assertTrue(!!group);
    return group;
  }

  function blockAll3pc(): ControlledRadioButtonElement {
    const blockAll3pc =
        page.shadowRoot.querySelector<ControlledRadioButtonElement>(
            '#blockAll3pc');
    assertTrue(!!blockAll3pc);
    return blockAll3pc;
  }

  function block3pcIncognito(): ControlledRadioButtonElement {
    const block3pcIncognito =
        page.shadowRoot.querySelector<ControlledRadioButtonElement>(
            '#block3pcIncognito');
    assertTrue(!!block3pcIncognito);
    return block3pcIncognito;
  }

  function createPage() {
    page = document.createElement('settings-cookies-page');
    document.body.appendChild(page);
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      settingsRefresh2026: 'true',
    });
  });

  setup(async function() {
    resetRouterForTesting();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    createPage();
    await microtasksFinished();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('RenderCards', function() {
    const radioGroup = thirdPartyCookieBlockingSettingGroup();
    assertTrue(radioGroup.hasAttribute('is-horizontal'));
    assertTrue(!!block3pcIncognito());
    assertTrue(!!blockAll3pc());
  });

  test('SubpageTitle', function() {
    assertEquals(
        page.i18n('thirdPartyCookiesPageTitle'),
        page.shadowRoot.querySelector('settings-subpage')!.pageTitle);
  });

  test('ElementVisibility', async function() {
    await flushTasks();
    assertTrue(isChildVisible(page, '#explanationText'));
    assertTrue(isChildVisible(page, '#generalControls'));
    assertTrue(isChildVisible(page, '#additionalProtections'));
    assertFalse(isChildVisible(page, '#cookiesHeader'));
    assertFalse(isChildVisible(page, '#siteRequestsHeader'));
    assertTrue(isChildVisible(page, '#exceptionHeader'));
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
        prefService.getPref('generated.third_party_cookie_blocking_setting')
            .value,
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
        prefService.getPref('generated.third_party_cookie_blocking_setting')
            .value,
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
});
