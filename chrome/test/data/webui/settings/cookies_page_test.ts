// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CookieControlsMode, ContentSetting, NetworkPredictionOptions, SettingsCollapseRadioButtonElement, ContentSettingsTypes,CookiePrimarySetting, SettingsCookiesPageElement, SITE_EXCEPTION_WILDCARD, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyElementInteractions, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

suite('CrSettingsCookiesPageTest', function() {
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

    assertTrue(isChildVisible(page, '#doNotTrack'));
    assertTrue(isChildVisible(page, '#preloadingLinkRow'));

    assertTrue(isChildVisible(page, '#allowThirdParty'));
    assertTrue(isChildVisible(page, '#blockThirdParty'));
    assertTrue(isChildVisible(page, '#blockThirdPartyIncognito'));
    assertFalse(isChildVisible(page, '#allowAll'));
    assertFalse(isChildVisible(page, '#blockAll'));
  });

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

  test('PreloadingSubLabel', async function() {
    assertTrue(isChildVisible(page, '#preloadingLinkRow'));
    // TODO(crbug.com/1385176): Remove after crbug.com/1385176 is launched.
    assertFalse(isChildVisible(page, '#preloadingToggle'));

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
    // case. See chrome/browser/prefetch/prefetch_prefs.h for more info.
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

// TODO(crbug.com/1378703): Remove after crbug/1378703 launched.
suite('CrSettingsCookiesPageTest_PrivacySandboxSettings4Disabled', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  function blockAll(): SettingsCollapseRadioButtonElement {
    return page.shadowRoot!.querySelector('#blockAll')!;
  }

  function blockThirdParty(): SettingsCollapseRadioButtonElement {
    return page.shadowRoot!.querySelector('#blockThirdParty')!;
  }

  function blockThirdPartyIncognito(): SettingsCollapseRadioButtonElement {
    return page.shadowRoot!.querySelector('#blockThirdPartyIncognito')!;
  }

  function allowAll(): SettingsCollapseRadioButtonElement {
    return page.shadowRoot!.querySelector('#allowAll')!;
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      // <if expr="chromeos_lacros">
      isSecondaryUser: false,
      // </if>
      isPrivacySandboxSettings4: false,
    });
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
    page.set('prefs.generated.cookie_session_only', {
      value: false,
    });
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', true);
    page.set(
        'prefs.generated.cookie_primary_setting.value',
        CookiePrimarySetting.ALLOW_ALL);
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('ElementVisibility', async function() {
    await flushTasks();
    assertFalse(isChildVisible(page, '#explanationText'));
    assertTrue(isChildVisible(page, '#generalControls'));
    assertTrue(isChildVisible(page, '#exceptionHeader'));
    assertTrue(isChildVisible(page, '#allowExceptionsList'));
    assertTrue(isChildVisible(page, '#sessionOnlyExceptionsList'));
    assertTrue(isChildVisible(page, '#blockExceptionsList'));
    assertTrue(isChildVisible(page, '#clearOnExit'));
    assertTrue(isChildVisible(page, '#doNotTrack'));
    assertTrue(isChildVisible(page, '#preloadingLinkRow'));
    assertTrue(isChildVisible(page, '#blockThirdPartyIncognito'));
  });

  test('CookiesRadioClicksRecorded', async function() {
    blockAll().click();
    let result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_BLOCK, result);
    testMetricsBrowserProxy.reset();

    blockThirdParty().click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_THIRD, result);
    testMetricsBrowserProxy.reset();

    blockThirdPartyIncognito().click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_INCOGNITO, result);
    testMetricsBrowserProxy.reset();

    allowAll().click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_ALL, result);
    testMetricsBrowserProxy.reset();
  });

  test('CookiesSessionOnlyClickRecorded', async function() {
    page.shadowRoot!.querySelector<HTMLElement>('#clearOnExit')!.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_SESSION, result);
  });

  // Checks that the sub label for "Clear on Exit" is shown for Lacros primary
  // profiles, and desktop platforms. It is not shown on Ash.
  // Note: Secondary Lacros profiles are tested in the suite
  // `CrSettingsCookiesPageTest_lacrosSecondaryProfile`.
  test('CookieSessionSublabel', function() {
    const clearOnExitRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>('#clearOnExit')!;
    let expectedSubLabel = '';
    // <if expr="not chromeos_ash">
    expectedSubLabel = page.i18n('cookiePageClearOnExitDesc');
    // </if>
    assertEquals(clearOnExitRow.subLabel, expectedSubLabel);
  });

  test('CookieSettingExceptions_Search', async function() {
    while (siteSettingsBrowserProxy.getCallCount('getExceptionList') < 3) {
      await flushTasks();
    }
    siteSettingsBrowserProxy.resetResolver('getExceptionList');

    const exceptionPrefs = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.COOKIES,
          [
            createRawSiteException('http://foo-block.com', {
              embeddingOrigin: '',
              setting: ContentSetting.BLOCK,
            }),
            createRawSiteException('http://foo-allow.com', {
              embeddingOrigin: '',
            }),
            createRawSiteException('http://foo-session.com', {
              embeddingOrigin: '',
              setting: ContentSetting.SESSION_ONLY,
            }),
          ]),
    ]);
    page.searchTerm = 'foo';
    siteSettingsBrowserProxy.setPrefs(exceptionPrefs);
    while (siteSettingsBrowserProxy.getCallCount('getExceptionList') < 3) {
      await flushTasks();
    }
    flush();

    const exceptionLists = page.shadowRoot!.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);

    for (const list of exceptionLists) {
      assertTrue(isChildVisible(list, 'site-list-entry'));
    }

    page.searchTerm = 'unrelated.com';
    flush();

    for (const list of exceptionLists) {
      assertFalse(isChildVisible(list, 'site-list-entry'));
    }
  });

  test('ExceptionLists_ReadOnly', async function() {
    // Check all exception lists are read only when the session only preference
    // reports as managed.
    page.set('prefs.generated.cookie_session_only', {
      value: true,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    let exceptionLists = page.shadowRoot!.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertTrue(!!list.readOnlyList);
    }

    // Return preference to unmanaged state and check all exception lists
    // are no longer read only.
    page.set('prefs.generated.cookie_session_only', {
      value: true,
    });
    exceptionLists = page.shadowRoot!.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertFalse(!!list.readOnlyList);
    }
  });

  test('ExceptionListsHaveCorrectCookieExceptionType', function() {
    for (const listId
             of ['#allowExceptionsList',
                 '#sessionOnlyExceptionsList',
                 '#blockExceptionsList',
    ]) {
      const exceptionList = page.shadowRoot!.querySelector(listId);
      assertTrue(!!exceptionList);
      assertEquals(
          'combined', exceptionList.getAttribute('cookies-exception-type'));
    }
  });

  test('BlockAll_ManagementSource', async function() {
    // Test that controlledBy for the blockAll_ preference is set to
    // the same value as the generated.cookie_session_only preference.
    page.set('prefs.generated.cookie_session_only', {
      value: true,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
    });
    flush();
    assertEquals(
        blockAll().pref!.controlledBy,
        chrome.settingsPrivate.ControlledBy.EXTENSION);

    page.set('prefs.generated.cookie_session_only', {
      value: true,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
    });
    assertEquals(
        blockAll().pref!.controlledBy,
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY);
  });

  test('privacySandboxToast', async function() {
    assertFalse(page.$.toast.open);

    // Disabling all cookies should display the privacy sandbox toast.
    blockAll().click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertTrue(page.$.toast.open);

    // Clicking the toast link should be recorded in UMA and should dismiss
    // the toast.
    page.$.toast.querySelector('cr-button')!.click();
    assertEquals(
        'Settings.PrivacySandbox.OpenedFromCookiesPageToast',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertFalse(page.$.toast.open);

    // Renabling 3P cookies for regular sessions should not display the toast.
    blockThirdPartyIncognito().click();
    await flushTasks();
    assertFalse(page.$.toast.open);
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));

    // The toast should not be displayed if the user has the privacy sandbox
    // APIs disabled.
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', false);
    blockAll().click();
    await flushTasks();
    assertFalse(page.$.toast.open);
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));

    // Disabling only 3P cookies should display the toast.
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', true);
    page.set(
        'prefs.generated.cookie_primary_setting.value',
        CookiePrimarySetting.ALLOW_ALL);
    blockThirdParty().click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    assertTrue(page.$.toast.open);

    // Reselecting a non-3P cookie blocking setting should hide the toast.
    allowAll().click();
    await flushTasks();
    assertFalse(page.$.toast.open);

    // Navigating away from the page should hide the toast, even if navigated
    // back to.
    blockAll().click();
    await flushTasks();
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
    page.set('prefs.privacy_sandbox.apis_enabled_v2.value', true);
    blockAll().click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertFalse(page.$.toast.open);
  });

  test('blockThirdPartyIncognitoSecondBulletPointText', function() {
    // Confirm the correct string is set.
    const cookiesPageBlockThirdPartyIncognitoBulTwoLabel =
        page.shadowRoot!
            .querySelector<HTMLElement>(
                '#cookiesPageBlockThirdPartyIncognitoBulTwo')!.innerText.trim();
    assertEquals(
        loadTimeData.getString('cookiePageBlockThirdIncognitoBulTwoFps'),
        cookiesPageBlockThirdPartyIncognitoBulTwoLabel);
  });
});

// TODO(crbug/1349370): Remove after crbug/1349370 is launched.
suite('CrSettingsCookiesPageTest_FirstPartySetsUIDisabled', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({firstPartySetsUIEnabled: false});
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

// TODO(crbug/1378703): Remove after crbug/1378703 or crbug/1349370 are
// launched.
suite(
    'CrSettingsCookiesPageTest_PrivacySandboxSettings4Disabled_FirstPartySetsUIDisabled',
    function() {
      let page: SettingsCookiesPageElement;
      let settingsPrefs: SettingsPrefsElement;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          isPrivacySandboxSettings4: false,
          firstPartySetsUIEnabled: false,
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
                    '#cookiesPageBlockThirdPartyIncognitoBulTwo')!.innerText
                .trim();
        assertEquals(
            loadTimeData.getString('cookiePageBlockThirdIncognitoBulTwo'),
            cookiesPageBlockThirdPartyIncognitoBulTwoLabel);
      });
    });

// <if expr="chromeos_lacros">
// TODO(crbug/1378703): Remove after crbug/1378703 launched.
suite('CrSettingsCookiesPageTest_lacrosSecondaryProfile', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isSecondaryUser: true,
      isPrivacySandboxSettings4: false,
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

  // Checks that the sub label for "Clear on Exit" is not shown for secondary
  // Lacros profiles.
  test('CookieSessionSublabel', function() {
    const clearOnExitRow =
        page.shadowRoot!.querySelector<CrLinkRowElement>('#clearOnExit')!;
    assertEquals(clearOnExitRow.subLabel, '');
  });
});
// </if>

// TODO(crbug.com/1385176): Remove after crbug.com/1385176 is launched.
suite('PreloadingDesktopSettingsSubPageDisabled', function() {
  let page: SettingsCookiesPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      showPreloadingSubPage: false,
    });

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

  test('NetworkPredictionClickRecorded', async function() {
    const preloadingToggle =
        page.shadowRoot!.querySelector<HTMLElement>('#preloadingToggle');
    assertTrue(!!preloadingToggle);
    preloadingToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.NETWORK_PREDICTION, result);
  });

  test('PreloadingToggleShown', function() {
    assertTrue(isChildVisible(page, '#preloadingToggle'));
    assertFalse(isChildVisible(page, '#preloadingLinkRow'));
  });
});
