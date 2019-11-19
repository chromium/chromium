// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-stream. */

import 'chrome://extensions/extensions.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestService} from './test_service.js';
import {testVisible} from './test_util.js';

suite('ExtensionsActivityLogStreamTest', function() {
  /**
   * Backing extension id, same id as the one in createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

  const activity1 = {
    extensionId: EXTENSION_ID,
    activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
    time: 1550101623113,
    args: JSON.stringify([null]),
    apiCall: 'testAPI.testMethod',
  };

  const activity2 = {
    extensionId: EXTENSION_ID,
    activityType: chrome.activityLogPrivate.ExtensionActivityType.DOM_EVENT,
    time: 1550101623116,
    args: JSON.stringify(['testArg']),
    apiCall: 'testAPI.DOMMethod',
    pageUrl: 'testUrl',
  };

  const contentScriptActivity = {
    extensionId: EXTENSION_ID,
    activityType:
        chrome.activityLogPrivate.ExtensionActivityType.CONTENT_SCRIPT,
    time: 1550101623115,
    args: JSON.stringify(['script1.js', 'script2.js']),
    apiCall: '',
  };

  /**
   * Extension activityLogStream created before each test.
   * @type {ActivityLogStream}
   */
  let activityLogStream;
  let proxyDelegate;
  let boundTestVisible;

  // Initialize an extension activity log item before each test.
  setup(function() {
    PolymerTest.clearBody();
    proxyDelegate = new TestService();

    activityLogStream = document.createElement('activity-log-stream');

    activityLogStream.extensionId = EXTENSION_ID;
    activityLogStream.delegate = proxyDelegate;
    boundTestVisible = testVisible.bind(null, activityLogStream);

    document.body.appendChild(activityLogStream);
  });

  teardown(function() {
    activityLogStream.remove();
  });

  // Returns a list of visible stream items. The not([hidden]) selector is
  // needed for iron-list as it reuses components but hides them when not in
  // use.
  function getStreamItems() {
    return activityLogStream.shadowRoot.querySelectorAll(
        'activity-log-stream-item:not([hidden])');
  }

  test('button toggles stream on/off', function() {
    flush();

    // Stream should be on when element is first attached to the DOM.
    boundTestVisible('.activity-subpage-header', true);
    boundTestVisible('#empty-stream-message', true);
    boundTestVisible('#stream-started-message', true);

    activityLogStream.$$('#toggle-stream-button').click();
    boundTestVisible('#stream-stopped-message', true);
  });

  test(
      'new activity events are only shown while the stream is started',
      function() {
        flush();
        proxyDelegate.getOnExtensionActivity().callListeners(activity1);

        flush();
        // One event coming in. Since the stream is on, we should be able to see
        // it.
        let streamItems = getStreamItems();
        expectEquals(1, streamItems.length);

        // Pause the stream.
        activityLogStream.$$('#toggle-stream-button').click();
        proxyDelegate.getOnExtensionActivity().callListeners(
            contentScriptActivity);

        flush();
        // One event was fired but the stream was paused, we should still see
        // only one item.
        streamItems = getStreamItems();
        expectEquals(1, streamItems.length);

        // Resume the stream.
        activityLogStream.$$('#toggle-stream-button').click();
        proxyDelegate.getOnExtensionActivity().callListeners(activity2);

        flush();
        streamItems = getStreamItems();
        expectEquals(2, streamItems.length);

        expectEquals(
            streamItems[0].$$('#activity-name').innerText,
            'testAPI.testMethod');
        expectEquals(
            streamItems[1].$$('#activity-name').innerText, 'testAPI.DOMMethod');
      });

  test('activities shown match search query', function() {
    flush();
    boundTestVisible('#empty-stream-message', true);

    proxyDelegate.getOnExtensionActivity().callListeners(activity1);
    proxyDelegate.getOnExtensionActivity().callListeners(activity2);

    flush();
    expectEquals(2, getStreamItems().length);

    const search = activityLogStream.$$('cr-search-field');
    assertTrue(!!search);

    // Search for the apiCall of |activity1|.
    search.setValue('testMethod');
    flush();

    const filteredStreamItems = getStreamItems();
    expectEquals(1, getStreamItems().length);
    expectEquals(
        filteredStreamItems[0].$$('#activity-name').innerText,
        'testAPI.testMethod');

    // search again, expect none
    search.setValue('not expecting any activities to match');
    flush();

    expectEquals(0, getStreamItems().length);
    boundTestVisible('#empty-stream-message', false);
    boundTestVisible('#empty-search-message', true);

    // Another activity comes in while the stream is listening but search
    // returns no results.
    proxyDelegate.getOnExtensionActivity().callListeners(contentScriptActivity);

    search.$$('#clearSearch').click();
    flush();

    // We expect 4 activities to appear as |contentScriptActivity| (which is
    // split into 2 items) should be processed and stored in the stream
    // regardless of the search input.
    expectEquals(4, getStreamItems().length);
  });

  test('content script events are split by content script names', function() {
    proxyDelegate.getOnExtensionActivity().callListeners(contentScriptActivity);

    flush();
    let streamItems = getStreamItems();
    expectEquals(2, streamItems.length);

    // We should see two items: one for every script called.
    expectEquals(streamItems[0].$$('#activity-name').innerText, 'script1.js');
    expectEquals(streamItems[1].$$('#activity-name').innerText, 'script2.js');
  });

  test('clicking on clear button clears the activity log stream', function() {
    proxyDelegate.getOnExtensionActivity().callListeners(activity1);

    flush();
    expectEquals(1, getStreamItems().length);
    boundTestVisible('.activity-table-headings', true);
    activityLogStream.$$('.clear-activities-button').click();

    flush();
    expectEquals(0, getStreamItems().length);
    boundTestVisible('.activity-table-headings', false);
  });
});
