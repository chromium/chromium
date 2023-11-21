// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookieControlsMode, ContentSetting, NetworkPredictionOptions, SettingsCollapseRadioButtonElement, ContentSettingsTypes, SettingsCookiesPageElement, SITE_EXCEPTION_WILDCARD, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyElementInteractions, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
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
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    // TODO(): Remove assertFalse checks after the feature is launched.
    await flushTasks();
    assertTrue(isChildVisible(page, '#explanationText'));
    assertTrue(isChildVisible(page, '#generalControls'));
    assertTrue(isChildVisible(page, '#exceptionHeader'));
    assertTrue(isChildVisible(page, '#allowExceptionsList'));
    assertFalse(isChildVisible(page, '#sessionOnlyExceptionsList'));
    assertFalse(isChildVisible(page, '#blockExceptionsList'));

    assertFalse(isChildVisible(page, '#clearOnExit'));
    assertFalse(isChildVisible(page, '#rollbackNotice'));

    assertTrue(isChildVisible(page, '#doNotTrack'));
    // TODO(b/296212999): Remove after b/296212999 is launched.
    assertTrue(isChildVisible(page, '#preloadingLinkRow'));

    assertTrue(isChildVisible(page, '#allowThirdParty'));
    assertTrue(isChildVisible(page, '#blockThirdParty'));
    assertTrue(isChildVisible(page, '#blockThirdPartyIncognito'));
    assertFalse(isChildVisible(page, '#allowAll'));
    assertFalse(isChildVisible(page, '#blockAll'));
  });

  // TODO(b/296212999): Remove after b/296212999 is launched.
  test('PreloadingClickRecorded', async function() {
    const linkRow =
        page.shadowRoot!.querySelector<HTMLElement>('#preloadingLinkRow');
    assertTrue(!!linkRow);
    linkRow.click();
    flush();

    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.NETWORK_PREDICTION, result);
    assertEquals(routes.PRELOADING, Router.getInstance().getCurrentRoute());
  });

  // TODO(b/296212999): Remove after b/296212999 is launched.
  test('PreloadingSubLabel', async function() {
    assertTrue(isChildVisible(page, '#preloadingLinkRow'));

    const preloadingPageLinkRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>('#preloadingLinkRow');
    assertTrue(!!preloadingPageLinkRow);

    page.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.DISABLED);
    flush();
    assertEquals(
        preloadingPageLinkRow.subLabel,
        page.i18n('preloadingPageNoPreloadingTitle'));

    page.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.EXTENDED);
    flush();
    assertEquals(
        preloadingPageLinkRow.subLabel,
        page.i18n('preloadingPageExtendedPreloadingTitle'));

    page.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.STANDARD);
    flush();
    assertEquals(
        preloadingPageLinkRow.subLabel,
        page.i18n('preloadingPageStandardPreloadingTitle'));

    // This value is deprecated, and users cannot change their prefs to this
    // value, but it is still the default value for the pref. It is treated the
    // as STANDARD and the "Standard preloading" sub label is applied for this
    // case. See chrome/browser/preloading/preloading_prefs.h for more info.
    page.setPrefValue(
        'net.network_prediction_options',
        NetworkPredictionOptions.WIFI_ONLY_DEPRECATED);
    flush();
    assertEquals(
        preloadingPageLinkRow.subLabel,
        page.i18n('preloadingPageStandardPreloadingTitle'));
  });

  test('ThirdPartyCookiesRadioClicksRecorded', async function() {
    blockThirdParty().click();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    let result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.THIRD_PARTY_COOKIES_BLOCK, result);
    testMetricsBrowserProxy.reset();

    blockThirdPartyIncognito().click();
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
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    // TODO(crbug.com/1378703): Check historgrams.
    assertTrue(page.$.toast.open);

    // Clicking the toast link should be recorded in UMA and should dismiss
    // the toast.
    page.$.toast.querySelector('cr-button')!.click();
    // TODO(crbug.com/1378703): Check historgrams.
    assertFalse(page.$.toast.open);

    // Renabling 3P cookies for regular sessions should not display the toast.
    blockThirdPartyIncognito().click();
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
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.BLOCK_THIRD_PARTY);
    // TODO(crbug.com/1378703): Check historgrams.
    assertTrue(page.$.toast.open);

    // Reselecting a non-3P cookie blocking setting should hide the toast.
    allowThirdParty().click();
    await flushTasks();
    assertEquals(
        page.getPref('profile.cookie_controls_mode.value'),
        CookieControlsMode.OFF);
    assertFalse(page.$.toast.open);

    // Navigating away from the page should hide the toast, even if navigated
    // back to.
    blockThirdParty().click();
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
    loadTimeData.overrideValues({
      isPrivacySandboxRestricted: true,
    });
    page.set('prefs.privacy_sandbox.m1.topics_enabled.value', true);
    blockThirdParty().click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertFalse(page.$.toast.open);
  });

  test('disabledFPSToggle', function() {
    // Confirm that when the user has not selected the block 3PC setting, the
    // FPS toggle is disabled.
    const firstPartySetsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#firstPartySetsToggle')!;
    blockThirdParty().click();
    flush();
    assertEquals(
        CookieControlsMode.BLOCK_THIRD_PARTY,
        page.prefs.profile.cookie_controls_mode.value);
    assertFalse(firstPartySetsToggle.disabled, 'expect toggle to be enabled');

    allowThirdParty().click();
    flush();
    assertEquals(
        CookieControlsMode.OFF, page.prefs.profile.cookie_controls_mode.value);
    assertTrue(firstPartySetsToggle.disabled, 'expect toggle to be disabled');

    blockThirdPartyIncognito().click();
    flush();
    assertEquals(
        CookieControlsMode.INCOGNITO_ONLY,
        page.prefs.profile.cookie_controls_mode.value);
    assertTrue(firstPartySetsToggle.disabled, 'expect toggle to be disabled');
  });

  test('blockThirdPartyIncognitoSecondBulletPointText', function() {
    // Confirm the correct string is set.
    const cookiesPageBlockThirdPartyIncognitoBulTwoLabel =
        page.shadowRoot!
            .querySelector<HTMLElement>(
                '#blockThirdPartyIncognitoBulTwo')!.innerText.trim();
    assertEquals(
        loadTimeData.getString('cookiePageBlockThirdIncognitoBulTwoFps'),
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
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

  test('ExceptionListReadOnly', function() {
    // Check that the exception list is read only when the preference reports as
    // managed.
    page.set('prefs.generated.cookie_default_content_setting', {
      value: ContentSetting.ALLOW,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    const exceptionList1 = page.shadowRoot!.querySelector('site-list');
    assertTrue(!!exceptionList1);
    assertTrue(!!exceptionList1.readOnlyList);

    // Return preference to unmanaged state and check that the exception list
    // is no longer read only.
    page.set('prefs.generated.cookie_default_content_setting', {
      value: ContentSetting.ALLOW,
    });
    const exceptionList2 = page.shadowRoot!.querySelector('site-list');
    assertTrue(!!exceptionList2);
    assertFalse(!!exceptionList2.readOnlyList);
  });

  test('ExceptionListHasCorrectCookieExceptionType', function() {
    const exceptionList = page.shadowRoot!.querySelector('site-list');
    assertTrue(!!exceptionList);
    assertEquals(
        'third-party', exceptionList.getAttribute('cookies-exception-type'));
  });
});

// TODO(crbug/1349370): Remove after crbug/1349370 is launched.
suite('FirstPartySetsUIDisabled', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      firstPartySetsUIEnabled: false,
      // FirstPartySetsUI does not exist in 3PCD.
      is3pcdCookieSettingsRedesignEnabled: false,
    });
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

// TODO(b/296212999): Remove after b/296212999 is launched.
suite('PreloadingSubpageMovedToPerformanceSettings', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPerformanceSettingsPreloadingSubpageEnabled: true,
    });
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

  test('PreloadingLinkRowNotShown', function() {
    assertFalse(isChildVisible(page, '#preloadingLinkRow'));
  });
});

suite('TrackingProtectionSettings', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({is3pcdCookieSettingsRedesignEnabled: true});
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-cookies-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    flush();
  });

  test('CheckVisibility', function() {
    // Page description
    assertTrue(isChildVisible(page, '#explanationText'));
    assertEquals(
        page.shadowRoot!.querySelector<HTMLAnchorElement>(
                            'a[href]')!.getAttribute('aria-description'),
        page.i18n('opensInNewTab'));

    // Advanced toggles
    assertTrue(isChildVisible(page, '#blockThirdPartyToggle'));
    assertTrue(isChildVisible(page, '#doNotTrack'));

    // Site Exception list
    assertFalse(isChildVisible(page, '#exceptionHeader'));
    assertFalse(isChildVisible(page, '#exceptionHeaderSubLabel'));
    assertTrue(isChildVisible(page, '#exceptionHeader3pcd'));
    assertTrue(isChildVisible(page, '#allowExceptionsList'));
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

suite('TrackingProtectionSettingsRollbackNotice', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showTrackingProtectionSettingsRollbackNotice: true,
      // This notice only shows outside of 3PCD.
      is3pcdCookieSettingsRedesignEnabled: false,
    });
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

  test('RollbackNoticeDisplayed', function() {
    assertTrue(isChildVisible(page, '#rollbackNotice'));
  });
});
