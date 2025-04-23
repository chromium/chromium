// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsCollapseRadioButtonElement, SettingsRadioGroupElement, SettingsCookiesPageElement} from 'chrome://settings/lazy_load.js';
import {CookieControlsMode, ContentSettingsTypes, SITE_EXCEPTION_WILDCARD, SiteSettingsPrefsBrowserProxyImpl,ThirdPartyCookieBlockingSetting} from 'chrome://settings/lazy_load.js';
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

  function thirdPartyCookieBlockingSettingGroup(): SettingsRadioGroupElement {
    const group = page.shadowRoot!.querySelector<SettingsRadioGroupElement>(
        '#thirdPartyCookieBlockingSettingGroup');
    assertTrue(!!group);
    return group;
  }

  function blockThirdParty(): SettingsCollapseRadioButtonElement {
    const blockThirdParty = page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>('#blockThirdParty');
    assertTrue(!!blockThirdParty);
    return blockThirdParty;
  }

  function blockThirdPartyIncognito(): SettingsCollapseRadioButtonElement {
    const blockThirdPartyIncognito = page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>('#blockThirdPartyIncognito');
    assertTrue(!!blockThirdPartyIncognito);
    return blockThirdPartyIncognito;
  }

  function allowThirdParty(): SettingsCollapseRadioButtonElement {
    const allowThirdParty = page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>('#allowThirdParty');
    assertTrue(!!allowThirdParty);
    return allowThirdParty;
  }

  function blockAll3pc(): SettingsCollapseRadioButtonElement {
    const blockAll3pc = page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>('#blockAll3pc');
    assertTrue(!!blockAll3pc);
    return blockAll3pc;
  }

  function block3pcIncognito(): SettingsCollapseRadioButtonElement {
    const block3pcIncognito = page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>('#block3pcIncognito');
    assertTrue(!!block3pcIncognito);
    return block3pcIncognito;
  }

  function createPage() {
    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;

    // Enable one of the PS APIs.
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    page.set(
      'prefs.profile.cookie_controls_mode.value', CookieControlsMode.OFF);
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
    // This test is for the pre-3PCD cookies page.
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: false,
      isAlwaysBlock3pcsIncognitoEnabled: true,
    });
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    createPage();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
  test('ElementVisibility_alwaysBlock3pcsIncognitoDisabled', async function() {
    loadTimeData.overrideValues({
      isAlwaysBlock3pcsIncognitoEnabled: false,
    });
    resetRouterForTesting();
    await createPage();
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
    assertTrue(isChildVisible(page, '#blockAll3pc'));
    assertTrue(isChildVisible(page, '#block3pcIncognito'));
    // By default these toggles should be hidden.
    assertFalse(isChildVisible(page, '#blockThirdPartyToggle'));
  });

  // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
  test('ThirdPartyCookiesRadioClicksRecorded_alwaysBlock3pcsIncognitoDisabled', async function() {
    loadTimeData.overrideValues({
      isAlwaysBlock3pcsIncognitoEnabled: false,
    });
    resetRouterForTesting();
    await createPage();

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

  test('thirdPartyCookiesRadioClicksRecorded', async function() {
        blockAll3pc().click();
        await eventToPromise(
            'selected-changed', thirdPartyCookieBlockingSettingGroup());
        assertEquals(
            page.getPref('generated.third_party_cookie_blocking_setting.value'),
            ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
        let result = await testMetricsBrowserProxy.whenCalled(
            'recordSettingsPageHistogram');
        assertEquals(
            PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK, result);
        assertEquals(
              'Settings.ThirdPartyCookies.Block',
              await testMetricsBrowserProxy.whenCalled('recordAction'));
        testMetricsBrowserProxy.reset();

        block3pcIncognito().click();
        await eventToPromise(
            'selected-changed', thirdPartyCookieBlockingSettingGroup());
        assertEquals(
            page.getPref('generated.third_party_cookie_blocking_setting.value'),
            ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
        result = await testMetricsBrowserProxy.whenCalled(
            'recordSettingsPageHistogram');
        assertEquals(
            PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK_IN_INCOGNITO,
            result);
        assertEquals(
              'Settings.ThirdPartyCookies.Allow',
              await testMetricsBrowserProxy.whenCalled('recordAction'));
        testMetricsBrowserProxy.reset();
      });

  // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
  test('privacySandboxToast_alwaysBlock3pcsIncognitoDisabled', async function() {
    loadTimeData.overrideValues({
      isAlwaysBlock3pcsIncognitoEnabled: false,
    });
    resetRouterForTesting();
    await createPage();
    assertFalse(page.$.toast.open);

    // Disabling third-party cookies should display the privacy sandbox toast.
    blockThirdParty().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    // TODO(crbug.com/40244046): Check histograms.
    assertTrue(page.$.toast.open);

    // Clicking the toast link should be recorded in UMA and should dismiss
    // the toast.
    page.$.toast.querySelector('cr-button')!.click();
    // TODO(crbug.com/40244046): Check histograms.
    assertFalse(page.$.toast.open);

    // Re-enabling 3P cookies for regular sessions should not display the toast.
    blockThirdPartyIncognito().click();
    await eventToPromise('selected-changed', primarySettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.INCOGNITO_ONLY);
    assertFalse(page.$.toast.open);

    // The toast should not be displayed if the user has any privacy sandbox
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
    // TODO(crbug.com/40244046): Check histograms.
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

  test('privacySandboxToast', async function() {
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: false,
    });
    resetRouterForTesting();
    await createPage();
    assertFalse(page.$.toast.open);

    // Disabling third-party cookies should display the privacy sandbox toast.
    page.set(
        'prefs.generated.third_party_cookie_blocking_setting.value',
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    blockAll3pc().click();
    await eventToPromise(
        'selected-changed', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    assertTrue(page.$.toast.open);

    // Re-enabling 3P cookies for regular sessions should not display the toast.
    block3pcIncognito().click();
    await eventToPromise(
        'selected-changed', thirdPartyCookieBlockingSettingGroup());
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
    await eventToPromise(
        'selected-changed', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    assertTrue(page.$.toast.open);

    // Reselecting a non-3P cookie blocking setting should hide the toast.
    block3pcIncognito().click();
    await eventToPromise(
        'selected-changed', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        page.getPref('generated.third_party_cookie_blocking_setting.value'),
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    assertFalse(page.$.toast.open);
  });

  // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
  test('privacySandboxToast_restrictedSandbox_alwaysBlock3pcsIncognitoDisabled', async function() {
    // No toast should be shown if the privacy sandbox is restricted
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
      isAlwaysBlock3pcsIncognitoEnabled: false,
    });
    resetRouterForTesting();
    await createPage();

    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    blockThirdParty().click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertFalse(page.$.toast.open);
  });

  // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
  test('disabledRWSToggle_alwaysBlock3pcsIncognitoDisabled', async () => {
    loadTimeData.overrideValues({
      isAlwaysBlock3pcsIncognitoEnabled: false,
    });
    resetRouterForTesting();
    await createPage();
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

  test('disabledRWSToggle', async () => {
// Verify the RWS toggle is enabled iff the user has selected block 3PCs.
    const relatedWebsiteSetsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#relatedWebsiteSetsToggle3pcSetting')!;
    blockAll3pc().click();
    await eventToPromise(
        'selected-changed', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY,
        page.prefs.generated.third_party_cookie_blocking_setting.value);
    assertFalse(
        relatedWebsiteSetsToggle.disabled, 'expect toggle to be enabled');

    block3pcIncognito().click();
    await eventToPromise(
        'selected-changed', thirdPartyCookieBlockingSettingGroup());
    await flushTasks();
    assertEquals(
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY,
        page.prefs.generated.third_party_cookie_blocking_setting.value);
    assertTrue(
        relatedWebsiteSetsToggle.disabled, 'expect toggle to be disabled');
  });

  // TODO(crbug.com/370008370): Remove once AlwaysBlock3pcsIncognito launched.
  test('blockThirdPartyIncognitoSecondBulletPointText_alwaysBlock3pcsIncognitoDisabled', async function() {
    loadTimeData.overrideValues({
      isAlwaysBlock3pcsIncognitoEnabled: false,
    });
    resetRouterForTesting();
    await createPage();

    // Confirm the correct string is set.
    const cookiesPageBlockThirdPartyIncognitoBulTwo =
        page.shadowRoot!
            .querySelector<HTMLElement>(
                '#blockThirdPartyIncognitoBulTwo');
    assertTrue(!!cookiesPageBlockThirdPartyIncognitoBulTwo);
    assertEquals(
        loadTimeData.getString('cookiePageBlockThirdIncognitoBulTwoRws'),
        cookiesPageBlockThirdPartyIncognitoBulTwo.innerText.trim());
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
