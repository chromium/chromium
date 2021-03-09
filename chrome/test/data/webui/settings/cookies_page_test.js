// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, ContentSettingsTypes, CookieControlsMode, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, Router, routes} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {flushTasks, isChildVisible, isVisible} from '../test_util.m.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting,createRawSiteException,createSiteSettingsPrefs} from './test_util.js';

// clang-format on

suite('CrSettingsCookiesPageTest', function() {
  /** @type {!TestSiteSettingsPrefsBrowserProxy} */
  let siteSettingsBrowserProxy;

  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {!SettingsCookiesPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: false,
      privacySandboxSettingsEnabled: false,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    document.body.innerHTML = '';
    page = /** @type {!SettingsCookiesPageElement} */ (
        document.createElement('settings-cookies-page'));
    page.prefs = {
      generated: {
        cookie_session_only: {value: false},
        cookie_primary_setting:
            {type: chrome.settingsPrivate.PrefType.NUMBER, value: 0},
      },
      privacy_sandbox: {
        apis_enabled: {value: true},
      }
    };
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('ElementVisibility', async function() {
    await flushTasks();
    assertFalse(isChildVisible(page, '#exceptionHeader'));
    assertTrue(isChildVisible(page, '#clearOnExit'));
    assertTrue(isChildVisible(page, '#doNotTrack'));
    assertTrue(isChildVisible(page, '#networkPrediction'));
    assertTrue(isChildVisible(page, '#blockThirdPartyIncognito'));
  });

  test('NetworkPredictionClickRecorded', async function() {
    page.$$('#networkPrediction').click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.NETWORK_PREDICTION, result);
  });

  test('CookiesSiteDataSubpageRoute', function() {
    page.$$('#site-data-trigger').click();
    assertEquals(
        Router.getInstance().getCurrentRoute(), routes.SITE_SETTINGS_SITE_DATA);
  });

  test('CookiesRadioClicksRecorded', async function() {
    page.$$('#blockAll').click();
    let result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_BLOCK, result);
    testMetricsBrowserProxy.reset();

    page.$$('#blockThirdParty').click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_THIRD, result);
    testMetricsBrowserProxy.reset();

    page.$$('#blockThirdPartyIncognito').click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_INCOGNITO, result);
    testMetricsBrowserProxy.reset();

    page.$$('#allowAll').click();
    result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_ALL, result);
    testMetricsBrowserProxy.reset();
  });

  test('CookiesSessionOnlyClickRecorded', async function() {
    page.$$('#clearOnExit').click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.COOKIES_SESSION, result);
  });

  test('CookieSettingExceptions_Search', async function() {
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
    await siteSettingsBrowserProxy.whenCalled('getExceptionList');
    flush();

    const exceptionLists = /** @type {!NodeList<!SiteListElement>} */ (
        page.shadowRoot.querySelectorAll('site-list'));
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
    let exceptionLists = page.shadowRoot.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertTrue(!!list.readOnlyList);
    }

    // Return preference to unmanaged state and check all exception lists
    // are no longer read only.
    page.set('prefs.generated.cookie_session_only', {
      value: true,
    });
    exceptionLists = page.shadowRoot.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertFalse(!!list.readOnlyList);
    }
  });

  test('BlockAll_ManagementSource', async function() {
    // Test that controlledBy for the blockAll_ preference is set to
    // the same value as the generated.cookie_session_only preference.
    const blockAll = page.$$('#blockAll');
    page.set('prefs.generated.cookie_session_only', {
      value: true,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
    });
    flush();
    assertEquals(
        blockAll.pref.controlledBy,
        chrome.settingsPrivate.ControlledBy.EXTENSION);

    page.set('prefs.generated.cookie_session_only', {
      value: true,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY
    });
    assertEquals(
        blockAll.pref.controlledBy,
        chrome.settingsPrivate.ControlledBy.DEVICE_POLICY);
  });

  test('NoPrivacySandboxToast', async function() {
    // Check that the privacy sandbox toast is not shown while the feature is
    // disabled.
    page.set('prefs.privacy_sandbox.apis_enabled.value', true);
    page.$$('#blockAll').click();

    await flushTasks();
    assertFalse(page.$$('#toast').open);

    // Reset the primary preference as the previous click will have changed it.
    page.set('prefs.generated.cookie_primary_setting.value', 0);
    page.$$('#blockThirdParty').click();

    await flushTasks();
    assertFalse(page.$$('#toast').open);
  });
});

suite('ContentSettingsRedesign', function() {
  /** @type {!SettingsCookiesPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: true,
    });
  });

  setup(function() {
    document.body.innerHTML = '';
    page = /** @type {!SettingsCookiesPageElement} */ (
        document.createElement('settings-cookies-page'));
    page.prefs = {
      generated: {
        cookie_session_only: {value: false},
        cookie_primary_setting:
            {type: chrome.settingsPrivate.PrefType.NUMBER, value: 0},
      },
    };
    document.body.appendChild(page);
  });

  test('HeaderVisibility', async function() {
    assertTrue(isChildVisible(page, '#exceptionHeader'));
  });
});

suite('PrivacySandboxEnabled', function() {
  /** @type {!SettingsCookiesPageElement} */
  let page;

  /** @type {!TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySandboxSettingsEnabled: true,
    });
  });

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    document.body.innerHTML = '';
    page = /** @type {!SettingsCookiesPageElement} */ (
        document.createElement('settings-cookies-page'));
    page.prefs = {
      generated: {
        cookie_session_only: {value: false},
        cookie_primary_setting:
            {type: chrome.settingsPrivate.PrefType.NUMBER, value: 0},
      },
      privacy_sandbox: {
        apis_enabled: {value: true},
      }
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('privacySandboxToast', async function() {
    assertFalse(page.$$('#toast').open);

    // Disabling all cookies should display the privacy sandbox toast.
    page.$$('#blockAll').click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertTrue(page.$$('#toast').open);

    // Clicking the toast link should be recorded in UMA and should dismiss
    // the toast.
    page.$$('#toast').querySelector('cr-button').click();
    assertEquals(
        'Settings.PrivacySandbox.OpenedFromCookiesPageToast',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    testMetricsBrowserProxy.resetResolver('recordAction');
    assertFalse(page.$$('#toast').open);

    // Renabling 3P cookies for regular sessions should not display the toast.
    page.$$('#blockThirdPartyIncognito').click();
    await flushTasks();
    assertFalse(page.$$('#toast').open);
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));

    // The toast should not be displayed if the user has the privacy sandbox
    // APIs disabled.
    page.set('prefs.privacy_sandbox.apis_enabled.value', false);
    page.$$('#blockAll').click();
    await flushTasks();
    assertFalse(page.$$('#toast').open);
    assertEquals(0, testMetricsBrowserProxy.getCallCount('recordAction'));

    // Disabling only 3P cookies should display the toast.
    page.set('prefs.privacy_sandbox.apis_enabled.value', true);
    page.set('prefs.generated.cookie_primary_setting.value', 0);
    page.$$('#blockThirdParty').click();
    assertEquals(
        'Settings.PrivacySandbox.Block3PCookies',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
    assertTrue(page.$$('#toast').open);

    // Reselecting a non-3P cookie blocking setting should hide the toast.
    page.$$('#allowAll').click();
    await flushTasks();
    assertFalse(page.$$('#toast').open);

    // Navigating away from the page should hide the toast, even if navigated
    // back to.
    page.$$('#blockAll').click();
    await flushTasks();
    assertTrue(page.$$('#toast').open);
    Router.getInstance().navigateTo(routes.BASIC);
    Router.getInstance().navigateTo(routes.COOKIES);
    await flushTasks();
    assertFalse(page.$$('#toast').open);
  });
});
