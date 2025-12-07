// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrExpandButtonElement, SettingsSiteSettingsPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, CookieControlsMode, ContentSettingsTypes, defaultSettingLabel, SettingsState, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, Route, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue, assertThrows} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

const redesignedPages: Route[] = [
  routes.SITE_SETTINGS_HANDLERS,
  routes.SITE_SETTINGS_NOTIFICATIONS,
  routes.SITE_SETTINGS_PDF_DOCUMENTS,
  routes.SITE_SETTINGS_PROTECTED_CONTENT,

  // TODO(crbug.com/40719916) After restructure add coverage for elements on
  // routes which depend on flags being enabled.
  // routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
  // routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
  // routes.SITE_SETTINGS_WINDOW_MANAGEMENT,

  // Doesn't contain toggle or radio buttons
  // routes.SITE_SETTINGS_AUTOMATIC_FULLSCREEN,
  // routes.SITE_SETTINGS_INSECURE_CONTENT,
  // routes.SITE_SETTINGS_ZOOM_LEVELS,
];

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

  function getCookiesLinkRow() {
    const basicContentList =
        page.shadowRoot!.querySelector('#basicContentList');
    assertTrue(!!basicContentList);
    const cookiesLinkRow =
        basicContentList.shadowRoot!.querySelector<CrLinkRowElement>(
            '#cookies');
    assertTrue(!!cookiesLinkRow);
    return cookiesLinkRow;
  }

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
  });

  test('CookiesLinkRowSublabelInModeB', async function() {
    // This test verifies the Tracking Protection rewind label.
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: true,
    });
    setupPage();
    const cookiesLinkRow = getCookiesLinkRow();

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

  test('CookiesLinkRowSublabel', async function() {
    loadTimeData.overrideValues({
      is3pcdCookieSettingsRedesignEnabled: false,
    });
    setupPage();
    const cookiesLinkRow = getCookiesLinkRow();

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
        loadTimeData.getString('thirdPartyCookiesLinkRowSublabelEnabled'),
        cookiesLinkRow.subLabel);

    page.set(
        'prefs.profile.cookie_controls_mode.value', CookieControlsMode.OFF);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('thirdPartyCookiesLinkRowSublabelEnabled'),
        cookiesLinkRow.subLabel);
  });

  test('NotificationsLinkRowSublabel', async function() {
    const basicPermissionsList =
        page.shadowRoot!.querySelector('#basicPermissionsList');
    assertTrue(!!basicPermissionsList);
    const notificationsLinkRow =
        basicPermissionsList.shadowRoot!.querySelector<CrLinkRowElement>(
            '#notifications')!;
    assertTrue(!!notificationsLinkRow);

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
    const advancedContentList =
        page.shadowRoot!.querySelector('#advancedContentList');
    assertTrue(!!advancedContentList);
    assertTrue(isChildVisible(advancedContentList, '#protected-content'));
  });

  test('SiteDataLinkRowSublabel', async function() {
    setupPage();
    const expandContent =
        page.shadowRoot!.querySelector<HTMLElement>('#expandContent');
    assertTrue(!!expandContent);
    expandContent.click();
    flush();

    const advancedContentList =
        page.shadowRoot!.querySelector('#advancedContentList');
    assertTrue(!!advancedContentList);
    const siteDataLinkRow =
        advancedContentList.shadowRoot!.querySelector<CrLinkRowElement>(
            '#site-data');
    assertTrue(!!siteDataLinkRow);

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

  test('UnusedSitePermissionsControlToggleUpdatesPrefs', function() {
    const unusedSitePermissionsRevocationToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#unusedSitePermissionsRevocationToggle');
    assertTrue(!!unusedSitePermissionsRevocationToggle);

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

    const safetyHubButton =
        page.shadowRoot!.querySelector<HTMLElement>('#safetyHubButton');
    assertTrue(!!safetyHubButton);
    safetyHubButton.click();
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

// Isolated ContentSettingsVisibility test suite due to significantly higher
// execution time (10-20x factor) of that specific tests compare to other
// sub-tests.
suite('ContentSettingsVisibility', function() {
  let page: SettingsSiteSettingsPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    page = document.createElement('settings-site-settings-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('ContentSettingsVisibility', async function() {
    // Ensure pages are visited so that HTML components are stamped.
    redesignedPages.forEach(route => Router.getInstance().navigateTo(route));
    await flushTasks();

    // All redesigned pages will use `settings-category-default-radio-group`,
    // except
    //   1. protocol handlers,
    //   2. pdf documents,
    //   3. protected content (is in its own element),
    //   4. notifications (is in its own element)
    //   5. geolocation (is in its own element)
    const expectedPagesCount = redesignedPages.length - 4;

    assertEquals(
        page.shadowRoot!
            .querySelectorAll('settings-category-default-radio-group')
            .length,
        expectedPagesCount);
  });
});

suite('WebPrintingNotShown', function() {
  test('navigateToWebPrinting', function() {
    assertThrows(
        () =>
            Router.getInstance().navigateTo(routes.SITE_SETTINGS_WEB_PRINTING));
  });
});
