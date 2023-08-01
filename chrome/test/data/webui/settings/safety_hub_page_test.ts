// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {ContentSettingsTypes, SafetyHubEvent, SettingsSafetyHubPageElement} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl, PasswordManagerPage, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
// clang-format on

suite('SafetyHubPage', function() {
  let testElement: SettingsSafetyHubPageElement;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;
  let passwordManagerProxy: TestPasswordManagerProxy;

  const unusedSitePermissionMockData = [{
    origin: 'www.example.com',
    permissions: [ContentSettingsTypes.CAMERA],
    expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
  }];

  setup(function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    passwordManagerProxy = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManagerProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    return flushTasks();
  });

  test('Unused Site Permissions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    safetyHubBrowserProxy.setUnusedSitePermissions([]);
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    await flushTasks();
    assertFalse(
        isChildVisible(testElement, 'settings-unused-site-permissions'));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(
        testElement, 'settings-safety-hub-unused-site-permissions'));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(
        testElement, 'settings-safety-hub-unused-site-permissions'));

    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(
        testElement, 'settings-safety-hub-unused-site-permissions'));
  });

  test('Password Card Clicked', async function() {
    testElement.$.passwords.click();

    // Ensure the Password Manager Check-up page is shown.
    const param = await passwordManagerProxy.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);
  });

  test('Version Card Clicked', function() {
    testElement.$.version.click();

    // Ensure the About page is shown.
    assertEquals(routes.ABOUT, Router.getInstance().getCurrentRoute());
  });

  test('Safe Browsing Card Clicked', function() {
    testElement.$.safeBrowsing.click();

    // Ensure the Security Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });
});
