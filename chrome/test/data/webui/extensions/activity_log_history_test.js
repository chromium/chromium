// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-history. */

import {ActivityLogPageState} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestService} from './test_service.js';
import {testVisible} from './test_util.js';

suite('ExtensionsActivityLogHistoryTest', function() {
  /**
   * Backing extension id, same id as the one in createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

  const testActivities = {
    activities: [
      {
        activityId: '299',
        activityType: 'api_call',
        apiCall: 'i18n.getUILanguage',
        args: 'null',
        count: 10,
        extensionId: EXTENSION_ID,
        time: 1541203132002.664
      },
      {
        activityId: '309',
        activityType: 'dom_access',
        apiCall: 'Storage.getItem',
        args: 'null',
        count: 35,
        extensionId: EXTENSION_ID,
        other: {domVerb: 'method'},
        pageTitle: 'Test Extension',
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1541203131994.837
      },
      {
        activityId: '308',
        activityType: 'dom_access',
        apiCall: 'Storage.setItem',
        args: 'null',
        count: 10,
        extensionId: EXTENSION_ID,
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1541203131994.837
      },
      {
        activityId: '301',
        activityType: 'api_call',
        apiCall: 'i18n.getUILanguage',
        args: 'null',
        count: 30,
        extensionId: EXTENSION_ID,
        time: 1541203172002.664
      },
    ]
  };

  // The first two activities of |testActivities|,
  const testExportActivities = {
    activities: testActivities.activities.slice(0, 2),
  };

  // Sample activities representing content script invocations. Activities with
  // missing args will not be processed.
  const testContentScriptActivities = {
    activities: [
      {
        activityId: '288',
        activityType: 'content_script',
        apiCall: '',
        args: `["script1.js","script2.js"]`,
        count: 1,
        extensionId: EXTENSION_ID,
        pageTitle: 'Test Extension',
        pageUrl: 'https://www.google.com/search'
      },
      {
        activityId: '290',
        activityType: 'content_script',
        apiCall: '',
        count: 1,
        extensionId: EXTENSION_ID,
        pageTitle: 'Test Extension',
        pageUrl: 'https://www.google.com/search'
      },
    ]
  };

  // Sample activities representing web requests. Activities with valid fields
  // in other.webRequest should be split into multiple entries; one for every
  // field. Activities with empty fields will have the group name be just the
  // web request API call.
  const testWebRequestActivities = {
    activities: [
      {
        activityId: '1337',
        activityType: 'web_request',
        apiCall: 'webRequest.onBeforeSendHeaders',
        args: 'null',
        count: 300,
        extensionId: EXTENSION_ID,
        other: {
          webRequest:
              `{"modified_request_headers":true, "added_request_headers":"a"}`
        },
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1546499283237.616
      },
      {
        activityId: '1339',
        activityType: 'web_request',
        apiCall: 'webRequest.noWebRequestObject',
        args: 'null',
        count: 3,
        extensionId: EXTENSION_ID,
        other: {},
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1546499283237.616
      },
    ]
  };

  /**
   * Extension activityLogHistory created before each test.
   * @type {ActivityLogHistory}
   */
  let activityLogHistory;
  let proxyDelegate;
  let boundTestVisible;

  function setupActivityLogHistory() {
    PolymerTest.clearBody();

    activityLogHistory = document.createElement('activity-log-history');
    boundTestVisible = testVisible.bind(null, activityLogHistory);

    activityLogHistory.extensionId = EXTENSION_ID;
    activityLogHistory.delegate = proxyDelegate;
    document.body.appendChild(activityLogHistory);

    return proxyDelegate.whenCalled('getExtensionActivityLog');
  }

  // Initialize an extension activity log before each test.
  setup(function() {
    proxyDelegate = new TestService();
  });

  teardown(function() {
    activityLogHistory.remove();
  });

  function getHistoryItems() {
    return activityLogHistory.shadowRoot.querySelectorAll(
        'activity-log-history-item');
  }

  // We know an item is expanded if its page-url-list is not hidden.
  function getExpandedItems() {
    return Array.from(getHistoryItems()).filter(item => {
      return item.$$('#page-url-list:not([hidden])');
    });
  }

  test('activities are present for extension', async function() {
    proxyDelegate.testActivities = testActivities;
    await setupActivityLogHistory();

    flush();

    boundTestVisible('#no-activities', false);
    boundTestVisible('#loading-activities', false);
    boundTestVisible('#activity-list', true);
    boundTestVisible('.activity-table-headings', true);

    const activityLogItems = getHistoryItems();
    expectEquals(activityLogItems.length, 3);

    // Test the order of the activity log items here. This test is in this
    // file because the logic to group activity log items by their API call
    // is in activity_log_history.js.
    expectEquals(
        activityLogItems[0].$$('#activity-key').innerText,
        'i18n.getUILanguage');
    expectEquals(activityLogItems[0].$$('#activity-count').innerText, '40');

    expectEquals(
        activityLogItems[1].$$('#activity-key').innerText, 'Storage.getItem');
    expectEquals(activityLogItems[1].$$('#activity-count').innerText, '35');

    expectEquals(
        activityLogItems[2].$$('#activity-key').innerText, 'Storage.setItem');
    expectEquals(activityLogItems[2].$$('#activity-count').innerText, '10');
  });

  test('activities shown match search query', async function() {
    proxyDelegate.testActivities = testActivities;
    await setupActivityLogHistory();

    const search = activityLogHistory.$$('cr-search-field');
    assertTrue(!!search);

    // Partial, case insensitive search for i18n.getUILanguage. Whitespace is
    // also appended to the search term to test trimming.
    search.setValue('getuilanguage   ');

    await proxyDelegate.whenCalled('getFilteredExtensionActivityLog');

    flush();
    const activityLogItems = getHistoryItems();
    // Since we searched for an API call, we expect only one match as
    // activity log entries are grouped by their API call.
    expectEquals(activityLogItems.length, 1);
    expectEquals(
        activityLogItems[0].$$('#activity-key').innerText,
        'i18n.getUILanguage');

    // Change search query so no results match.
    proxyDelegate.resetResolver('getFilteredExtensionActivityLog');
    search.setValue('query that does not match any activities');

    await proxyDelegate.whenCalled('getFilteredExtensionActivityLog');

    flush();

    boundTestVisible('#no-activities', true);
    boundTestVisible('#loading-activities', false);
    boundTestVisible('#activity-list', false);
    expectEquals(0, getHistoryItems().length);

    proxyDelegate.resetResolver('getExtensionActivityLog');

    // Finally, we clear the search query via the #clearSearch button.
    // We should see all the activities displayed.
    search.$$('#clearSearch').click();

    await proxyDelegate.whenCalled('getExtensionActivityLog');

    flush();
    expectEquals(3, getHistoryItems().length);
  });

  test('script names shown for content script activities', async function() {
    proxyDelegate.testActivities = testContentScriptActivities;
    await setupActivityLogHistory();

    flush();
    const activityLogItems = getHistoryItems();

    // One activity should be shown for each content script name.
    expectEquals(activityLogItems.length, 2);

    expectEquals(
        activityLogItems[0].$$('#activity-key').innerText, 'script1.js');
    expectEquals(
        activityLogItems[1].$$('#activity-key').innerText, 'script2.js');
  });

  test(
      'other.webRequest fields shown for web request activities',
      async function() {
        proxyDelegate.testActivities = testWebRequestActivities;
        await setupActivityLogHistory();

        flush();
        const activityLogItems = getHistoryItems();

        // First activity should be split into two groups as it has two actions
        // recorded in the other.webRequest object. We display the names of
        // these actions along with the API call. Second activity should fall
        // back to using just the API call as the key. Hence we end up with
        // three activity log items.
        const expectedItemKeys = [
          'webRequest.onBeforeSendHeaders (added_request_headers)',
          'webRequest.onBeforeSendHeaders (modified_request_headers)',
          'webRequest.noWebRequestObject'
        ];
        const expectedNumItems = expectedItemKeys.length;

        expectEquals(activityLogItems.length, expectedNumItems);

        for (let i = 0; i < expectedNumItems; ++i) {
          expectEquals(
              activityLogItems[i].$$('#activity-key').innerText,
              expectedItemKeys[i]);
        }
      });

  test('expand/collapse all', async function() {
    proxyDelegate.testActivities = testActivities;
    await setupActivityLogHistory();

    flush();

    const expandableItems =
        Array.from(getHistoryItems())
            .filter(item => item.$$('cr-expand-button:not([hidden])'));
    expectEquals(2, expandableItems.length);

    // All items should be collapsed by default.
    expectEquals(0, getExpandedItems().length);

    // Click the dropdown toggle, then expand all.
    activityLogHistory.$$('#more-actions').click();
    activityLogHistory.$$('#expand-all-button').click();

    flush();
    expectEquals(2, getExpandedItems().length);

    // Collapse all items.
    activityLogHistory.$$('#more-actions').click();
    activityLogHistory.$$('#collapse-all-button').click();

    flush();
    expectEquals(0, getExpandedItems().length);
  });

  test('export activities', async function() {
    // |testExportActivities| stringified and sorted by timestamp.
    const expectedRawActivityData =
        '[{"activityId":"309","activityType":"dom_access","apiCall":"Storage.' +
        'getItem","args":"null","count":35,"extensionId":"aaaaaaaaaaaaaaaaaaa' +
        'aaaaaaaaaaaaa","other":{"domVerb":"method"},"pageTitle":"Test Extens' +
        'ion","pageUrl":"chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/' +
        'index.html","time":1541203131994.837},{"activityId":"299","activityT' +
        'ype":"api_call","apiCall":"i18n.getUILanguage","args":"null","count"' +
        ':10,"extensionId":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","time":15412031' +
        '32002.664}]';

    proxyDelegate.testActivities = testExportActivities;
    await setupActivityLogHistory();
    flush();

    activityLogHistory.$$('#more-actions').click();
    activityLogHistory.$$('#export-button').click();

    const [actualRawActivityData, actualFileName] =
        await proxyDelegate.whenCalled('downloadActivities');

    expectEquals(expectedRawActivityData, actualRawActivityData);
    expectEquals(`exported_activity_log_${EXTENSION_ID}.json`, actualFileName);
  });

  test(
      'clicking on the delete button for an activity row deletes that row',
      async function() {
        proxyDelegate.testActivities = testActivities;
        await setupActivityLogHistory();

        flush();
        const activityLogItems = getHistoryItems();

        expectEquals(activityLogItems.length, 3);
        proxyDelegate.resetResolver('getExtensionActivityLog');
        activityLogItems[0].$$('#activity-delete').click();

        // We delete the first item so we should only have one item left. This
        // chaining reflects the API calls made from activity_log.js.
        await proxyDelegate.whenCalled('deleteActivitiesById');
        await proxyDelegate.whenCalled('getExtensionActivityLog');

        flush();
        expectEquals(2, getHistoryItems().length);
      });

  test(
      'message shown when no activities present for extension',
      async function() {
        // Spoof an API call and pretend that the extension has no activities.
        proxyDelegate.testActivities = {activities: []};
        await setupActivityLogHistory();

        flush();

        boundTestVisible('#no-activities', true);
        boundTestVisible('#loading-activities', false);
        boundTestVisible('#activity-list', false);
        expectEquals(0, getHistoryItems().length);
      });

  test('message shown when activities are being fetched', async function() {
    // Spoof an API call and pretend that the extension has no activities.
    proxyDelegate.testActivities = {activities: []};
    await setupActivityLogHistory();

    // Pretend the activity log is still loading.
    activityLogHistory.pageState_ = ActivityLogPageState.LOADING;

    flush();

    boundTestVisible('#no-activities', false);
    boundTestVisible('#loading-activities', true);
    boundTestVisible('#activity-list', false);
  });

  test(
      'clicking on clear button clears the activity log history',
      async function() {
        proxyDelegate.testActivities = testActivities;
        await setupActivityLogHistory();

        flush();

        expectEquals(3, getHistoryItems().length);
        activityLogHistory.$$('.clear-activities-button').click();

        await proxyDelegate.whenCalled('deleteActivitiesFromExtension');

        flush();
        boundTestVisible('#no-activities', true);
        boundTestVisible('.activity-table-headings', false);
        expectEquals(0, getHistoryItems().length);
      });
});
