// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {CardInfo, CardState, ContentSettingsTypes, SafetyHubBrowserProxyImpl, SafetyHubEvent,SettingsSafetyHubPageElement} from 'chrome://settings/lazy_load.js';
import {LifetimeBrowserProxyImpl, PasswordManagerImpl, PasswordManagerPage, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
// clang-format on

suite('SafetyHubPage', function() {
  let testElement: SettingsSafetyHubPageElement;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;
  let passwordManagerProxy: TestPasswordManagerProxy;

  const notificationPermissionMockData = [{
    origin: 'www.example.com',
    notificationInfoString: 'About 1 notifications a day',
  }];

  const unusedSitePermissionMockData = [{
    origin: 'www.example.com',
    permissions: [ContentSettingsTypes.CAMERA],
    expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
  }];

  const passwordCardMockData: CardInfo = {
    header: '2 compromised passwords',
    subheader: 'You should change these now',
    state: CardState.WARNING,
  };

  const versionCardMockData: CardInfo = {
    header: 'Chrome is up to date',
    subheader: 'Checked just now',
    state: CardState.SAFE,
  };

  const safeBrowsingCardMockData: CardInfo = {
    header: 'Safe Browsing is off',
    subheader: 'An Extension turned off Safe Browsing',
    state: CardState.INFO,
  };

  setup(function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    safetyHubBrowserProxy.setPasswordCardData(passwordCardMockData);
    safetyHubBrowserProxy.setVersionCardData(versionCardMockData);
    safetyHubBrowserProxy.setSafeBrowsingCardData(safeBrowsingCardMockData);
    SafetyHubBrowserProxyImpl.setInstance(safetyHubBrowserProxy);

    passwordManagerProxy = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManagerProxy);

    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    return flushTasks();
  });

  function assertNoRecommendationState(shouldBeVisible: boolean) {
    assertEquals(
        shouldBeVisible, isChildVisible(testElement, '#emptyStateModule'));
    assertEquals(
        shouldBeVisible, isChildVisible(testElement, '#userEducationModule'));
  }

  test(
      'No Recommendation State Visibility With Unused Site Permissions Module',
      async function() {
        // The element is visible when there is nothing to review.
        assertNoRecommendationState(true);

        // The element becomes hidden if the is any module that needs attention.
        webUIListenerCallback(
            SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
            unusedSitePermissionMockData);
        await flushTasks();
        assertNoRecommendationState(false);

        // Once hidden, it remains hidden as other modules are visible.
        webUIListenerCallback(
            SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
        await flushTasks();
        assertNoRecommendationState(false);
      });

  test(
      'No Recommendation State Visibility With Notification Permissions Module',
      async function() {
        // The element is visible when there is nothing to review.
        assertNoRecommendationState(true);

        // The element becomes hidden if the is any module that needs attention.
        webUIListenerCallback(
            SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
            notificationPermissionMockData);
        await flushTasks();
        assertNoRecommendationState(false);

        // Once hidden, it remains hidden as other modules are visible.
        webUIListenerCallback(
            SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
        await flushTasks();
        assertNoRecommendationState(false);
      });

  test(
      'No Recommendation State Visibility With Extensions Module',
      async function() {
        // The element is visible when there is nothing to review.
        assertTrue(isChildVisible(testElement, '#emptyStateModule'));

        // The element becomes hidden if the is any module that needs attention.
        webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 1);
        await flushTasks();
        assertFalse(isChildVisible(testElement, '#emptyStateModule'));

        // Returns when extension module goes away.
        webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 0);
        await flushTasks();
        assertTrue(isChildVisible(testElement, '#emptyStateModule'));
      });

  test('Unused Site Permissions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    const unusedSitePermissionsElementTag =
        'settings-safety-hub-unused-site-permissions';
    assertFalse(isChildVisible(testElement, unusedSitePermissionsElementTag));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, unusedSitePermissionsElementTag));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(testElement, unusedSitePermissionsElementTag));

    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, unusedSitePermissionsElementTag));
  });

  test('Notification Permissions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    const notificationPermissionsElementTag =
        'settings-safety-hub-notification-permissions-module';
    assertFalse(isChildVisible(testElement, notificationPermissionsElementTag));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        notificationPermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, notificationPermissionsElementTag));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(testElement, notificationPermissionsElementTag));

    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        notificationPermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, notificationPermissionsElementTag));
  });

  test('Extensions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    assertFalse(
        isChildVisible(testElement, 'settings-safety-hub-extensions-module'));

    // The element becomes visible if the there are extensions to review.
    webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 2);
    await flushTasks();
    assertTrue(
        isChildVisible(testElement, 'settings-safety-hub-extensions-module'));

    // Once visible, it goes away if all extensions are handled.
    webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 0);
    await flushTasks();
    assertFalse(
        isChildVisible(testElement, 'settings-safety-hub-extensions-module'));
  });

  test('Password Card', async function() {
    assertTrue(isChildVisible(testElement, '#passwords'));

    // Card header and subheader should be what the browser proxy provides.
    assertEquals(
        testElement.$.passwords.shadowRoot!.querySelector(
                                               '#header')!.textContent!.trim(),
        passwordCardMockData.header);
    assertEquals(
        testElement.$.passwords.shadowRoot!.querySelector('#subheader')!
            .textContent!.trim(),
        passwordCardMockData.subheader);
  });

  test('Password Card Clicked', async function() {
    testElement.$.passwords.click();

    // Ensure the Password Manager Check-up page is shown.
    const param = await passwordManagerProxy.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);
  });

  test('Version Card', async function() {
    assertTrue(isChildVisible(testElement, '#version'));

    // Card header and subheader should be what the browser proxy provides.
    assertEquals(
        testElement.$.version.shadowRoot!.querySelector(
                                             '#header')!.textContent!.trim(),
        versionCardMockData.header);
    assertEquals(
        testElement.$.version.shadowRoot!.querySelector(
                                             '#subheader')!.textContent!.trim(),
        versionCardMockData.subheader);
  });

  test('Version Card Clicked When No Update Waiting', function() {
    testElement.$.version.click();

    // Ensure the About page is shown.
    assertEquals(routes.ABOUT, Router.getInstance().getCurrentRoute());
  });

  test('Version Card Clicked When Update Waiting', async function() {
    const versionCardMockData: CardInfo = {
      header: 'Chrome is not up to date',
      subheader: 'Relaunch to update',
      state: CardState.WARNING,
    };
    safetyHubBrowserProxy.setVersionCardData(versionCardMockData);
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    await flushTasks();

    testElement.$.version.click();

    // Ensure the browser is restarted.
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('Safe Browsing Card', async function() {
    assertTrue(isChildVisible(testElement, '#safeBrowsing'));

    // Card header and subheader should be what the browser proxy provides.
    assertEquals(
        testElement.$.safeBrowsing.shadowRoot!.querySelector('#header')!
            .textContent!.trim(),
        safeBrowsingCardMockData.header);
    assertEquals(
        testElement.$.safeBrowsing.shadowRoot!.querySelector('#subheader')!
            .textContent!.trim(),
        safeBrowsingCardMockData.subheader);
  });

  test('Safe Browsing Card Clicked', function() {
    testElement.$.safeBrowsing.click();

    // Ensure the Security Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });
});
