// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, CookieControlsMode, ContentSettingsTypes, defaultSettingLabel, NotificationSetting, SettingsSiteSettingsPageElement, SafetyHubBrowserProxyImpl, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrLinkRowElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('SiteSettingsPage', function() {
  let page: SettingsSiteSettingsPageElement;

  function setupPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-settings-page');
    page.prefs = {
      generated: {
        notification: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: NotificationSetting.ASK,
        },
        cookie_default_content_setting: {
          type: chrome.settingsPrivate.PrefType.STRING,
          value: ContentSetting.ALLOW,
        },
      },
      profile: {
        cookie_controls_mode: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookieControlsMode.OFF,
        },
      },
    };
    document.body.appendChild(page);
    flush();
  }

  setup(setupPage);

  teardown(function() {
    page.remove();
  });

  test('DefaultLabels', function() {
    assertEquals('a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b'));
    assertEquals('b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b'));
    assertEquals('a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b', 'c'));
    assertEquals('b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b', 'c'));
    assertEquals(
        'c', defaultSettingLabel(ContentSetting.SESSION_ONLY, 'a', 'b', 'c'));
    assertEquals(
        'c', defaultSettingLabel(ContentSetting.DEFAULT, 'a', 'b', 'c'));
    assertEquals('c', defaultSettingLabel(ContentSetting.ASK, 'a', 'b', 'c'));
    assertEquals(
        'c',
        defaultSettingLabel(ContentSetting.IMPORTANT_CONTENT, 'a', 'b', 'c'));
  });

  test('AntiAbuseLinkRowHidden', async function() {
    loadTimeData.overrideValues({
      privateStateTokensEnabled: false,
    });
    setupPage();
    assertFalse(isChildVisible(
        page.$.advancedContentList, `#${ContentSettingsTypes.ANTI_ABUSE}`));
  });

  // TODO(crbug.com/1378703): Remove the test once PrivacySandboxSettings4
  // has been rolled out.
  test('CookiesLinkRowLabel', function() {
    const labelExpected =
        loadTimeData.getString('thirdPartyCookiesLinkRowLabel');
    const labelActual =
        page.shadowRoot!.querySelector('#basicContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#cookies')!.label;
    assertEquals(labelExpected, labelActual);
  });

  test('CookiesLinkRowSublabel', async function() {
    setupPage();
    const cookiesLinkRow =
        page.shadowRoot!.querySelector('#basicContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#cookies')!;

    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.BLOCK_THIRD_PARTY);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('thirdPartyCookiesLinkRowSublabelDisabled'),
        cookiesLinkRow.subLabel);

    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.INCOGNITO_ONLY);
    await flushTasks();
    assertEquals(
        loadTimeData.getString(
            'thirdPartyCookiesLinkRowSublabelDisabledIncognito'),
        cookiesLinkRow.subLabel);

    page.set(
        'prefs.profile.cookie_controls_mode.value', CookieControlsMode.OFF);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('thirdPartyCookiesLinkRowSublabelEnabled'),
        cookiesLinkRow.subLabel);
  });

  test('NotificationsLinkRowSublabel', async function() {
    const notificationsLinkRow =
        page.shadowRoot!.querySelector('#basicPermissionsList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#notifications')!;

    page.set('prefs.generated.notification.value', NotificationSetting.BLOCK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsBlocked'),
        notificationsLinkRow.subLabel);

    page.set(
        'prefs.generated.notification.value',
        NotificationSetting.QUIETER_MESSAGING);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsPartial'),
        notificationsLinkRow.subLabel);

    page.set('prefs.generated.notification.value', NotificationSetting.ASK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsAllowed'),
        notificationsLinkRow.subLabel);
  });

  test('ProtectedContentRow', function() {
    setupPage();
    page.shadowRoot!.querySelector<HTMLElement>('#expandContent')!.click();
    flush();
    assertTrue(isChildVisible(
        page.shadowRoot!.querySelector('#advancedContentList')!,
        '#protected-content'));
  });

  // TODO(crbug/1378703): Remove after crbug/1378703 launched.
  test('SiteDataLinkRow', function() {
    setupPage();
    page.shadowRoot!.querySelector<HTMLElement>('#expandContent')!.click();
    flush();

    assertTrue(isChildVisible(
        page.shadowRoot!.querySelector('#advancedContentList')!, '#site-data'));
  });

  test('SiteDataLinkRowSublabel', async function() {
    setupPage();
    page.shadowRoot!.querySelector<HTMLElement>('#expandContent')!.click();
    flush();

    const siteDataLinkRow =
        page.shadowRoot!.querySelector('#advancedContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#site-data')!;

    page.set(
        'prefs.generated.cookie_default_content_setting.value',
        ContentSetting.BLOCK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsSiteDataBlockedSubLabel'),
        siteDataLinkRow.subLabel);

    page.set(
        'prefs.generated.cookie_default_content_setting.value',
        ContentSetting.SESSION_ONLY);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsSiteDataClearOnExitSubLabel'),
        siteDataLinkRow.subLabel);

    page.set(
        'prefs.generated.cookie_default_content_setting.value',
        ContentSetting.ALLOW);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsSiteDataAllowedSubLabel'),
        siteDataLinkRow.subLabel);
  });
});

// TODO(crbug/1378703): Remove after crbug/1378703 launched.
suite('PrivacySandboxSettings4Disabled', function() {
  let page: SettingsSiteSettingsPageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  const testLabels: string[] = ['test label 1', 'test label 2'];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isPrivacySandboxSettings4: false,
    });
  });

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]!);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('SiteDataLinkRow', function() {
    page.shadowRoot!.querySelector<HTMLElement>('#expandContent')!.click();
    flush();

    assertFalse(isChildVisible(
        page.shadowRoot!.querySelector('#advancedContentList')!, '#site-data'));
  });

  test('CookiesLinkRowLabel', function() {
    const labelExpected = loadTimeData.getString('siteSettingsCookies');
    const labelActual =
        page.shadowRoot!.querySelector('#basicContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#cookies')!.label;
    assertEquals(labelExpected, labelActual);
  });

  test('CookiesLinkRowSublabel', async function() {
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    const cookiesLinkRow =
        page.shadowRoot!.querySelector('#basicContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#cookies')!;
    assertEquals(testLabels[0], cookiesLinkRow.subLabel);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(testLabels[1], cookiesLinkRow.subLabel);
  });
});

const unusedSitePermissionMockData = [{
  origin: 'www.example.com',
  permissions: [ContentSettingsTypes.CAMERA],
  expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
}];

suite('UnusedSitePermissionsReview', function() {
  let page: SettingsSiteSettingsPageElement;
  let siteSettingsPermissionsBrowserProxy: TestSafetyHubBrowserProxy;

  setup(function() {
    siteSettingsPermissionsBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(siteSettingsPermissionsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('VisibilityWithChangingPermissionList', async function() {
    // The element is not visible when there is nothing to review.
    siteSettingsPermissionsBrowserProxy.setUnusedSitePermissions([]);
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();
    assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        'unused-permission-review-list-maybe-changed',
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, 'settings-unused-site-permissions'));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback('unused-permission-review-list-maybe-changed', []);
    await flushTasks();
    assertTrue(isChildVisible(page, 'settings-unused-site-permissions'));

    webUIListenerCallback(
        'unused-permission-review-list-maybe-changed',
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, 'settings-unused-site-permissions'));
  });
});

/**
 * If feature is not enabled, the UI should not be shown regardless of whether
 * there would be unused site permissions for the user to review.
 *
 * TODO(crbug/1345920): Remove after crbug/1345920 launched.
 */
suite('UnusedSitePermissionsReviewDisabled', function() {
  let page: SettingsSiteSettingsPageElement;
  let siteSettingsPermissionsBrowserProxy: TestSafetyHubBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      safetyCheckUnusedSitePermissionsEnabled: false,
    });
  });

  setup(function() {
    siteSettingsPermissionsBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(siteSettingsPermissionsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('InvisibleWhenFeatureDisabled', async function() {
    siteSettingsPermissionsBrowserProxy.setUnusedSitePermissions([]);
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();

    assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));
  });

  test('InvisibleWhenFeatureDisabledWithItemsToReview', async function() {
    siteSettingsPermissionsBrowserProxy.setUnusedSitePermissions(
        unusedSitePermissionMockData);
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();

    assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));
  });
});
