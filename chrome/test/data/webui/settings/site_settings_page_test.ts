// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrExpandButtonElement, SettingsSiteSettingsPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, CookieControlsMode, ContentSettingsTypes, defaultSettingLabel, SettingsState, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

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
          value: SettingsState.LOUD,
        },
        cookie_default_content_setting: {
          type: chrome.settingsPrivate.PrefType.STRING,
          value: ContentSetting.ALLOW,
        },
        geolocation: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: SettingsState.LOUD,
        },
      },
      profile: {
        cookie_controls_mode: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: CookieControlsMode.OFF,
        },
      },
      safety_hub: {
        unused_site_permissions_revocation: {
          enabled: {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
        },
      },
      compose: {
        proactive_nudge_enabled: {
          enabled: {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
        },
      },
      tracking_protection: {
        block_all_3pc_toggle_enabled: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
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

  test('CookiesLinkRowSublabel', async function() {
    // This test verifies the pre-3PCD label.
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: false,
    });
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

  test('CookiesLinkRowSublabelInModeB', async function() {
    // This test verifies the Tracking Protection rewind label.
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: true,
      isTrackingProtectionUxEnabled: false,
    });
    setupPage();
    const cookiesLinkRow =
        page.shadowRoot!.querySelector('#basicContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#cookies')!;

    page.set(
        'prefs.tracking_protection.block_all_3pc_toggle_enabled.value', true);
    await flushTasks;
    assertTrue(Boolean(page.get(
        'prefs.tracking_protection.block_all_3pc_toggle_enabled.value')));
    assertEquals(
        loadTimeData.getString('thirdPartyCookiesLinkRowSublabelDisabled'),
        cookiesLinkRow.subLabel);

    page.set(
        'prefs.tracking_protection.block_all_3pc_toggle_enabled.value', false);
    await flushTasks;
    assertFalse(Boolean(page.get(
        'prefs.tracking_protection.block_all_3pc_toggle_enabled.value')));
    assertEquals(
        loadTimeData.getString('thirdPartyCookiesLinkRowSublabelLimited'),
        cookiesLinkRow.subLabel);
  });

  test('TrackingProtectionLinkRowSubLabel', async function() {
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: true,
      isTrackingProtectionUxEnabled: true,
    });
    setupPage();
    const cookiesLinkRow =
        page.shadowRoot!.querySelector('#basicContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#cookies')!;
    assertEquals(
        loadTimeData.getString('trackingProtectionLinkRowSubLabel'),
        cookiesLinkRow.subLabel);

    // Even if cookie controls mode changes, sub-label stays the same.
    page.set(
        'prefs.profile.cookie_controls_mode.value',
        CookieControlsMode.BLOCK_THIRD_PARTY);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('trackingProtectionLinkRowSubLabel'),
        cookiesLinkRow.subLabel);

    page.set(
        'prefs.profile.cookie_controls_mode.value', CookieControlsMode.OFF);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('trackingProtectionLinkRowSubLabel'),
        cookiesLinkRow.subLabel);
  });

  test('NotificationsLinkRowSublabel', async function() {
    const notificationsLinkRow =
        page.shadowRoot!.querySelector('#basicPermissionsList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#notifications')!;

    page.set('prefs.generated.notification.value', SettingsState.BLOCK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsBlocked'),
        notificationsLinkRow.subLabel);

    page.set('prefs.generated.notification.value', SettingsState.QUIET);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsAskQuiet'),
        notificationsLinkRow.subLabel);

    page.set('prefs.generated.notification.value', SettingsState.LOUD);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsAskLoud'),
        notificationsLinkRow.subLabel);
  });

  test('ProtectedContentRow', async function() {
    setupPage();
    const expandButton =
        page.shadowRoot!.querySelector<CrExpandButtonElement>('#expandContent');
    assertTrue(!!expandButton);
    expandButton.click();
    await expandButton.updateComplete;
    assertTrue(isChildVisible(
        page.shadowRoot!.querySelector('#advancedContentList')!,
        '#protected-content'));
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
        loadTimeData.getString('siteSettingsSiteDataDeleteOnExitSubLabel'),
        siteDataLinkRow.subLabel);

    page.set(
        'prefs.generated.cookie_default_content_setting.value',
        ContentSetting.ALLOW);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsSiteDataAllowedSubLabel'),
        siteDataLinkRow.subLabel);
  });

  test('StorageAccessLinkRow', function() {
    assertTrue(isChildVisible(
        page.shadowRoot!.querySelector('#basicPermissionsList')!,
        '#storage-access'));
  });

  test('AutomaticFullscreenRow', async function() {
    const expandButton =
        page.shadowRoot!.querySelector<CrExpandButtonElement>('#expandContent');
    assertTrue(!!expandButton);
    expandButton.click();
    await expandButton.updateComplete;
    assertTrue(isChildVisible(
      page.shadowRoot!.querySelector('#advancedContentList')!,
      '#automatic-fullscreen'));
  });

  // TODO(crbug.com/40267370): Remove after SafetyHub is launched.
  test('UnusedSitePermissionsControlToggleExists', function() {
    assertTrue(isChildVisible(page, '#unusedSitePermissionsRevocationToggle'));
  });

  test('UnusedSitePermissionsControlToggleUpdatesPrefs', function() {
    const unusedSitePermissionsRevocationToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#unusedSitePermissionsRevocationToggle')!;

    unusedSitePermissionsRevocationToggle.click();
    flush();
    assertFalse(Boolean(page.get(
        'prefs.safety_hub.unused_site_permissions_revocation.enabled.value')));

    unusedSitePermissionsRevocationToggle.click();
    flush();
    assertTrue(Boolean(page.get(
        'prefs.safety_hub.unused_site_permissions_revocation.enabled.value')));
  });
});

const unusedSitePermissionMockData = [{
  origin: 'www.example.com',
  permissions: [ContentSettingsTypes.CAMERA],
  expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
}];

suite('UnusedSitePermissionsReview', function() {
  let page: SettingsSiteSettingsPageElement;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;

  setup(async function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(safetyHubBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();
  });

  test('VisibilityWithChangingPermissionList', async function() {
    // The element is not visible when there is nothing to review.
    assertFalse(isChildVisible(page, '#safetyHubModule'));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, '#safetyHubModule'));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(page, '#safetyHubModule'));

    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, '#safetyHubModule'));
  });

  test('Button Click', async function() {
    // The element becomes visible if the list of permissions isn't empty.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();

    page.shadowRoot!.querySelector<HTMLElement>('#safetyHubButton')!.click();
    // Ensure the safety hub page is shown.
    assertEquals(routes.SAFETY_HUB, Router.getInstance().getCurrentRoute());
  });

  test('InvisibleWhenGuestMode', async function() {
    loadTimeData.overrideValues({
      isGuest: true,
    });

    // The element is not visible since it is guest mode.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertFalse(isChildVisible(page, '#safetyHubModule'));

    // Reset loadTimeData values.
    loadTimeData.overrideValues({
      isGuest: false,
    });
  });
});

// TODO(crbug.com/40267370): Remove after crbug/1443466 launched.
suite('UnusedSitePermissionsReviewSafetyHubDisabled', function() {
  let page: SettingsSiteSettingsPageElement;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSafetyHub: false,
    });
  });

  setup(async function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(safetyHubBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();
  });

  test('VisibilityWithChangingPermissionList', async function() {
    // The element is not visible when there is nothing to review.
    assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, 'settings-unused-site-permissions'));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(page, 'settings-unused-site-permissions'));

    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, 'settings-unused-site-permissions'));
  });

  test('InvisibleWhenGuestMode', async function() {
    loadTimeData.overrideValues({
      isGuest: true,
    });

    // The element is not visible since it is guest mode.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));

    // Reset loadTimeData values.
    loadTimeData.overrideValues({
      isGuest: false,
    });
  });
});

/**
 * If feature is not enabled, the UI should not be shown regardless of whether
 * there would be unused site permissions for the user to review.
 *
 * TODO(crbug.com/40232296): Remove after crbug/1345920 launched.
 */
suite('UnusedSitePermissionsReviewDisabled', function() {
  let page: SettingsSiteSettingsPageElement;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      safetyCheckUnusedSitePermissionsEnabled: false,
    });
  });

  setup(function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(safetyHubBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('InvisibleWhenFeatureDisabled', async function() {
    safetyHubBrowserProxy.setUnusedSitePermissions([]);
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();

    assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));
  });

  test('InvisibleWhenFeatureDisabledWithItemsToReview', async function() {
    safetyHubBrowserProxy.setUnusedSitePermissions(
        unusedSitePermissionMockData);
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();

    assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));
  });
});

// TODO(crbug.com/40267370): Remove after SafetyHub is launched.
suite('SafetyHubDisabled', function() {
  let page: SettingsSiteSettingsPageElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSafetyHub: false,
    });
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    flush();
  });

  test('NoUnusedSitePermissionsControlToggle', function() {
    assertFalse(
        Boolean(page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#unusedSitePermissionsRevocationToggle')));
  });
});

/**
 * If unused site permissions feature is not enabled, but abusive notification
 * revocation is enabled, the UI should be shown if there are permissions for
 * the user to review.
 *
 * TODO(crbug.com/328773301): Remove after
 * SafetyHubAbusiveNotificationRevocation is launched.
 */
suite('AbusiveNotificationsEnabledUnusedSitePermissionsDisabled', function() {
  let page: SettingsSiteSettingsPageElement;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableSafetyHub: false,
      safetyCheckUnusedSitePermissionsEnabled: false,
      safetyHubAbusiveNotificationRevocationEnabled: true,
    });
  });

  setup(async function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(safetyHubBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    await flushTasks();
  });

  test(
      'VisibleWithItemsToReviewUnusedSitePermissionsDisabled',
      async function() {
        // The element is not visible when there is nothing to review.
        assertFalse(isChildVisible(page, 'settings-unused-site-permissions'));

        // The element becomes visible if the list of permissions is no longer
        // empty.
        webUIListenerCallback(
            SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
            unusedSitePermissionMockData);
        await flushTasks();
        assertTrue(isChildVisible(page, 'settings-unused-site-permissions'));
      });
});
