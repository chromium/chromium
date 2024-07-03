// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActivityLogExtensionPlaceholder, ExtensionsActivityLogElement} from 'chrome://extensions/extensions.js';
import {navigation, Page} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo, testVisible} from './test_util.js';

/** @fileoverview Suite of tests for extensions-activity-log. */
suite('ExtensionsActivityLogTest', function() {
  /**
   * Backing extension id, same id as the one in
   * createExtensionInfo
   */
  const EXTENSION_ID: string = 'a'.repeat(32);

  /**
   * Extension activityLog created before each test.
   */
  let activityLog: ExtensionsActivityLogElement;

  /**
   * Backing extension info for the activity log.
   */
  let extensionInfo: chrome.developerPrivate.ExtensionInfo|
      ActivityLogExtensionPlaceholder;

  let proxyDelegate: TestService;
  let boundTestVisible: (selector: string, expectedVisible: boolean) => void;

  const testActivities = {activities: []};

  const activity1 = {
    extensionId: EXTENSION_ID,
    activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
    time: 1550101623113,
    args: JSON.stringify([null]),
    apiCall: 'testAPI.testMethod',
  };

  // Initialize an extension activity log before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Give this a large enough height that the tabs will be visible.
    document.body.style.height = '300px';

    activityLog = document.createElement('extensions-activity-log');
    boundTestVisible = testVisible.bind(null, activityLog);

    extensionInfo = createExtensionInfo({
      id: EXTENSION_ID,
    });
    activityLog.extensionInfo = extensionInfo;

    proxyDelegate = new TestService();
    activityLog.delegate = proxyDelegate;
    proxyDelegate.testActivities = testActivities;
    document.body.appendChild(activityLog);

    activityLog.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

    // Wait until we have finished making the call to fetch the activity log.
    return proxyDelegate.whenCalled('getExtensionActivityLog');
  });

  teardown(function() {
    activityLog.remove();
  });

  // Returns a list of visible stream items. The not([hidden]) selector is
  // needed for iron-list as it reuses components but hides them when not in
  // use.
  function getStreamItems() {
    return activityLog.shadowRoot!.querySelector('activity-log-stream')!
        .shadowRoot!.querySelectorAll('activity-log-stream-item:not([hidden])');
  }

  test('clicking on back button navigates to the details page', function() {
    flush();

    let currentPage = null;
    navigation.addListener(newPage => {
      currentPage = newPage;
    });

    activityLog.$.closeButton.click();
    assertDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: EXTENSION_ID});
  });

  test(
      'clicking on back button for a placeholder page navigates to list view',
      function() {
        activityLog.extensionInfo = {id: EXTENSION_ID, isPlaceholder: true};

        flush();

        let currentPage = null;
        navigation.addListener(newPage => {
          currentPage = newPage;
        });

        activityLog.$.closeButton.click();
        assertDeepEquals(currentPage, {page: Page.LIST});
      });

  test('tab transitions', async () => {
    await microtasksFinished();

    // Default view should be the history view.
    boundTestVisible('activity-log-history', true);

    // Navigate to the activity log stream.
    activityLog.$.tabs.selected = 1;
    await microtasksFinished();

    // One activity is recorded and should appear in the stream.
    proxyDelegate.getOnExtensionActivity().callListeners(activity1);
    flush();

    boundTestVisible('activity-log-stream', true);
    assertEquals(1, getStreamItems().length);

    // Navigate back to the activity log history tab.
    activityLog.$.tabs.selected = 0;

    // Expect a refresh of the activity log.
    await proxyDelegate.whenCalled('getExtensionActivityLog');
    await microtasksFinished();
    flush();
    boundTestVisible('activity-log-history', true);

    // Another activity is recorded, but should not appear in the stream as
    // the stream is inactive.
    proxyDelegate.getOnExtensionActivity().callListeners(activity1);
    flush();

    activityLog.$.tabs.selected = 1;
    await microtasksFinished();

    // The one activity in the stream should have persisted between tab
    // switches.
    assertEquals(1, getStreamItems().length);
  });
});
