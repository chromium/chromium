// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension-manager unit tests. Unlike
 * extension_manager_test.js, these tests are not interacting with the real
 * chrome.developerPrivate API.
 */

import 'chrome://extensions/extensions.js';

import type {ExtensionsManagerElement} from 'chrome://extensions/extensions.js';
import {navigation, Page, Service} from 'chrome://extensions/extensions.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('ExtensionManagerUnitTest', function() {
  let manager: ExtensionsManagerElement;
  let service: TestService;

  const testActivities = {activities: []};

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    service = new TestService();
    Service.setInstance(service);

    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait until Manager calls fetches data and initializes itself before
    // making any assertions.
    return Promise.all([
      service.whenCalled('getExtensionsInfo'),
      service.whenCalled('getProfileConfiguration'),
    ]);
  });

  /**
   * Trigger an event that indicates that an extension was installed.
   */
  function simulateExtensionInstall(
      info: chrome.developerPrivate.ExtensionInfo) {
    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.INSTALLED,
      extensionInfo: info,
    });
  }

  test('UpdateFromActivityLog', function() {
    service.testActivities = testActivities;

    const extension = createExtensionInfo();
    simulateExtensionInstall(extension);
    const secondExtension = createExtensionInfo({
      id: 'b'.repeat(32),
    });
    simulateExtensionInstall(secondExtension);

    assertTrue(manager.showActivityLog);
    navigation.navigateTo({
      page: Page.ACTIVITY_LOG,
      extensionId: extension.id,
    });

    const activityLog =
        manager.shadowRoot!.querySelector('extensions-activity-log');
    assertTrue(!!activityLog);  // View should now be present.
    assertEquals(extension.id, activityLog.extensionInfo.id);

    // Test that updates to different extensions does not change which
    // extension the activity log points to. Regression test for
    // https://crbug.com/924373.
    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.PREFS_CHANGED,
      extensionInfo: secondExtension,
    });

    assertEquals(extension.id, activityLog.extensionInfo.id);
  });
});
