// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {NotificationsPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SafetyHubBrowserProxyImpl, SafetyHubEvent, SettingsState, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefService, PrefsBrowserProxy, resetPageVisibilityForTesting, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

const NOTIFICATIONS_PREF = 'generated.notification';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: NOTIFICATIONS_PREF,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: SettingsState.CPSS,
    },
  ];
}

function createPref(
    category: ContentSettingsTypes,
    contentSetting: ContentSetting): SiteSettingsPref {
  return createSiteSettingsPrefs(
      [
        createContentSettingTypeToValuePair(
            category, createDefaultContentSetting({
              setting: contentSetting,
            })),
      ],
      []);
}

suite('NotificationsPage', function() {
  let page: NotificationsPageElement;
  let prefService: PrefService;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  function createPage() {
    page = document.createElement('settings-notifications-page');
    document.body.appendChild(page);
    return flushTasks();
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    return createPage();
  });

  teardown(function() {
    page.remove();
  });

  test('NotificationPage', function() {
    const notificationRadioGroup =
        page.shadowRoot!.querySelector('#notificationRadioGroup');
    assertTrue(!!notificationRadioGroup);

    const categorySettingExceptions =
        page.shadowRoot!.querySelector('category-setting-exceptions');
    assertTrue(!!categorySettingExceptions);
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.NOTIFICATIONS, categorySettingExceptions.category);
  });

  test('notificationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ASK));

    const cpssRadioGroup =
        page.shadowRoot!.querySelector('settings-radio-group');
    assertTrue(!!cpssRadioGroup);

    const radioGroup = page.shadowRoot!.querySelector<HTMLElement>(
        'settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(isVisible(radioGroup));
    assertTrue(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.CPSS, prefService.getPref(NOTIFICATIONS_PREF).value);

    const blockNotification =
        radioGroup.shadowRoot!.querySelector<HTMLElement>('#blockRadioOption');
    assertTrue(!!blockNotification);
    blockNotification.click();
    await flushTasks();
    assertFalse(isVisible(cpssRadioGroup));

    const askForNotification =
        radioGroup.shadowRoot!.querySelector<HTMLElement>('#askRadioOption');
    assertTrue(!!askForNotification);
    askForNotification.click();
    await flushTasks();
    assertTrue(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.CPSS, prefService.getPref(NOTIFICATIONS_PREF).value);
  });

  test('cpssRadioGroup', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ASK));

    const cpssRadioGroup =
        page.shadowRoot!.querySelector('settings-radio-group');
    assertTrue(!!cpssRadioGroup);
    assertEquals(SettingsState.CPSS.toString(), cpssRadioGroup.selected);
    assertEquals(
        SettingsState.CPSS, prefService.getPref(NOTIFICATIONS_PREF).value);

    const quietRadioButton = cpssRadioGroup.querySelector<HTMLElement>(
        `settings-collapse-radio-button[name="${SettingsState.QUIET}"]`);
    assertTrue(!!quietRadioButton);
    quietRadioButton.click();
    await flushTasks();
    assertEquals(
        SettingsState.QUIET, prefService.getPref(NOTIFICATIONS_PREF).value);
    assertEquals(SettingsState.QUIET.toString(), cpssRadioGroup.selected);

    await prefService.setPrefValue(NOTIFICATIONS_PREF, SettingsState.LOUD);
    await flushTasks();
    assertEquals(SettingsState.LOUD.toString(), cpssRadioGroup.selected);
  });
});

suite('NotificationPermissionReview', function() {
  let page: NotificationsPageElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let siteSettingsBrowserProxy: TestSafetyHubBrowserProxy;

  const oneElementMockData = [{
    origin: 'www.example.com',
    notificationInfoString: 'About 4 notifications a day',
  }];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    siteSettingsBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    // return createPage();
  });

  teardown(function() {
    page.remove();
  });

  function createPage() {
    page = document.createElement('settings-notifications-page');
    document.body.appendChild(page);
    return flushTasks();
  }

  test('InvisibleWhenGuestMode', async function() {
    loadTimeData.overrideValues({isGuest: true});
    resetPageVisibilityForTesting();
    resetRouterForTesting();
    await createPage();

    // The UI should remain invisible even when there's an event that the
    // notification permissions may have changed.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        oneElementMockData);
    await flushTasks();
    assertFalse(isChildVisible(page, '#safetyHubEntryPoint'));

    // Set guest mode back to false.
    loadTimeData.overrideValues({isGuest: false});
    resetPageVisibilityForTesting();
    resetRouterForTesting();
  });

  test('VisibilityWithChangingPermissionList', async function() {
    // The element is not visible when there is nothing to review.
    await createPage();
    assertFalse(isChildVisible(page, '#safetyHubEntryPoint'));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        oneElementMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, '#safetyHubEntryPoint'));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(page, '#safetyHubEntryPoint'));
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        oneElementMockData);
    await flushTasks();
    assertTrue(isChildVisible(page, '#safetyHubEntryPoint'));
  });
});
