// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {NotificationsPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl, SettingsState, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, resetRouterForTesting, resetPageVisibilityForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

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

suite(`NotificationsPage`, function() {
  let page: NotificationsPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    page = document.createElement('settings-notifications-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  }

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

    const blockNotification = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#disabledRadioOption');
    assertTrue(!!blockNotification);
    blockNotification.click();
    await flushTasks();
    assertFalse(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.BLOCK, page.get('prefs.generated.notification.value'));

    const allowNotification = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#enabledRadioOption');
    assertTrue(!!allowNotification);
    allowNotification.click();
    await flushTasks();
    assertTrue(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.CPSS, page.get('prefs.generated.notification.value'));
  });
});

// TODO(crbug.com/340743074): Remove tests after
// `PermissionSiteSettingsRadioButton` launched.
suite(`NotificationsPageWithNestedRadioButton`, function() {
  let page: NotificationsPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enablePermissionSiteSettingsRadioButton: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    page = document.createElement('settings-notifications-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  }

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

    assertTrue(isChildVisible(page, '#notificationRadioGroup'));

    const cpssRadioGroup =
        page.shadowRoot!.querySelector('#notificationCpssRadioGroup');
    assertTrue(!!cpssRadioGroup);
    assertTrue(isVisible(cpssRadioGroup));

    const blockNotification =
        page.shadowRoot!.querySelector<HTMLElement>('#notificationBlock');
    assertTrue(!!blockNotification);
    blockNotification.click();
    await flushTasks();
    assertFalse(isVisible(cpssRadioGroup));

    const allowNotification = page.shadowRoot!.querySelector<HTMLElement>(
        '#notificationAskRadioButton');
    assertTrue(!!allowNotification);
    allowNotification.click();
    await flushTasks();
    assertTrue(isVisible(cpssRadioGroup));
  });
});

suite('NotificationPermissionReview', function() {
  let page: NotificationsPageElement;
  let siteSettingsBrowserProxy: TestSafetyHubBrowserProxy;

  const oneElementMockData = [{
    origin: 'www.example.com',
    notificationInfoString: 'About 4 notifications a day',
  }];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

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
