// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension-manager unit tests. Unlike
 * extension_manager_test.js, these tests are not interacting with the real
 * chrome.developerPrivate API.
 */

import {navigation, Page, Service} from 'chrome://extensions/extensions.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

window.extension_manager_unit_tests = {};
extension_manager_unit_tests.suiteName = 'ExtensionManagerUnitTest';
/** @enum {string} */
extension_manager_unit_tests.TestNames = {
  UpdateFromActivityLog: 'update from activity log',
};

suite(extension_manager_unit_tests.suiteName, function() {
  /** @type {Manager} */
  let manager;

  /** @type {TestService} */
  let service;

  const testActivities = {activities: []};

  setup(function() {
    PolymerTest.clearBody();

    service = new TestService();
    Service.instance_ = service;

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
   * @param {!chrome.developerPrivate.ExtensionInfo} info
   */
  function simulateExtensionInstall(info) {
    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.INSTALLED,
      extensionInfo: info,
    });
  }

  test(
      assert(extension_manager_unit_tests.TestNames.UpdateFromActivityLog),
      function() {
        service.testActivities = testActivities;

        const extension = createExtensionInfo();
        simulateExtensionInstall(extension);
        const secondExtension = createExtensionInfo({
          id: 'b'.repeat(32),
        });
        simulateExtensionInstall(secondExtension);

        expectTrue(manager.showActivityLog);
        navigation.navigateTo({
          page: Page.ACTIVITY_LOG,
          extensionId: extension.id,
        });

        const activityLog = manager.$$('extensions-activity-log');
        assertTrue(!!activityLog);  // View should now be present.
        expectEquals(extension.id, activityLog.extensionInfo.id);

        // Test that updates to different extensions does not change which
        // extension the activity log points to. Regression test for
        // https://crbug.com/924373.
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.PREFS_CHANGED,
          extensionInfo: secondExtension,
        });

        expectEquals(extension.id, activityLog.extensionInfo.id);
      });
});
