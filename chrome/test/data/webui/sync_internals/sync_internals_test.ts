// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrTreeElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {getAboutInfoForTest} from 'chrome://sync-internals/about.js';
import {setAllNodesForTest} from 'chrome://sync-internals/chrome_sync.js';
import {setupSyncResultsListForTest} from 'chrome://sync-internals/search.js';
import {assertEquals, assertFalse, assertGE, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

/**
 * Checks aboutInfo's details section for the specified field.
 * @param isValid Whether the field is valid.
 * @param key The name of the key to search for in details.
 * @param value The expected value if |key| is found.
 * @return whether the field was found in the details.
 */
function hasInDetails(isValid: boolean, key: string, value: string): boolean {
  const details = getAboutInfoForTest().details;
  if (!details) {
    return false;
  }
  for (const detail of details) {
    if (!detail.data) {
      continue;
    }
    for (const obj of detail.data) {
      if (obj.stat_name === key) {
        return (obj.stat_status !== 'uninitialized') === isValid &&
            obj.stat_value === value;
      }
    }
  }
  return false;
}

/**
 * Constant hard-coded value to return from mock getAllNodes.
 * @const
 */
const HARD_CODED_ALL_NODES = [{
  'nodes': [
    {
      'ATTACHMENT_METADATA': '',
      'BASE_SERVER_SPECIFICS': {},
      'BASE_VERSION': '1396470970810000',
      'CTIME': 'Wednesday, December 31, 1969 4:00:00 PM',
      'ID': 'sZ:ADqtAZwzF4GOIyvkI2enSI62AU5p/7MNmvuSSyf7yXJ1SkJwpp1YL' +
          '6bbMkF8inzqW+EO6n2aPJ/uXccW9GHxorBlnKoZAWHVzg==',
      'IS_DEL': false,
      'IS_DIR': true,
      'IS_UNAPPLIED_UPDATE': false,
      'IS_UNSYNCED': false,
      'LOCAL_EXTERNAL_ID': '0',
      'METAHANDLE': 387,
      'MTIME': 'Wednesday, December 31, 1969 4:00:00 PM',
      'NON_UNIQUE_NAME': 'Autofill',
      'PARENT_ID': 'r',
      'SERVER_CTIME': 'Wednesday, December 31, 1969 4:00:00 PM',
      'SERVER_IS_DEL': false,
      'SERVER_IS_DIR': true,
      'SERVER_MTIME': 'Wednesday, December 31, 1969 4:00:00 PM',
      'SERVER_NON_UNIQUE_NAME': 'Autofill',
      'SERVER_PARENT_ID': 'r',
      'SERVER_SPECIFICS': {'autofill': {'usage_timestamp': []}},
      'SERVER_UNIQUE_POSITION': 'INVALID[]',
      'SERVER_VERSION': '1396470970810000',
      'SERVER_VERSION_TIME': '0',
      'SPECIFICS': {'autofill': {'usage_timestamp': []}},
      'SYNCING': false,
      'TRANSACTION_VERSION': '1',
      'UNIQUE_BOOKMARK_TAG': '',
      'UNIQUE_CLIENT_TAG': '',
      'UNIQUE_POSITION': 'INVALID[]',
      'UNIQUE_SERVER_TAG': 'google_chrome_autofill',
      'isDirty': false,
      'dataType': 'Autofill',
    },
    {
      'ATTACHMENT_METADATA': '',
      'BASE_SERVER_SPECIFICS': {},
      'BASE_VERSION': '1394241139528639',
      'CTIME': 'Friday, March 7, 2014 5:12:19 PM',
      'ID': 'sZ:ADqtAZwzc/ol1iaz+yNLjjWak9PBE0o/hATzpqJsyq/HX2xzV2f88' +
          'FaOrT7HDE4tyn7zx2LWgkAFvZfCA5mOy4p0XFgiY0L+mw==',
      'IS_DEL': false,
      'IS_DIR': false,
      'IS_UNAPPLIED_UPDATE': false,
      'IS_UNSYNCED': false,
      'LOCAL_EXTERNAL_ID': '0',
      'METAHANDLE': 2989,
      'MTIME': 'Friday, March 7, 2014 5:12:19 PM',
      'NON_UNIQUE_NAME': 'autofill_entry|Email|rlsynctet2',
      'PARENT_ID': 'sZ:ADqtAZwzF4GOIyvkI2enSI62AU5p/7MNmvuSSyf7yXJ1Sk' +
          'Jwpp1YL6bbMkF8inzqW+EO6n2aPJ/uXccW9GHxorBlnKoZAWHVzg==',
      'SERVER_CTIME': 'Friday, March 7, 2014 5:12:19 PM',
      'SERVER_IS_DEL': false,
      'SERVER_IS_DIR': false,
      'SERVER_MTIME': 'Friday, March 7, 2014 5:12:19 PM',
      'SERVER_NON_UNIQUE_NAME': 'autofill_entry|Email|rlsynctet2',
      'SERVER_PARENT_ID': 'sZ:ADqtAZwzF4GOIyvkI2enSI62AU5p/7MNmvuSSyf' +
          '7yXJ1SkJwpp1YL6bbMkF8inzqW+EO6n2aPJ/uXccW9GHxorBlnKoZAWHVzg==',
      'SERVER_SPECIFICS': {
        'autofill': {
          'name': 'Email',
          'usage_timestamp': ['13038713887000000', '13038713890000000'],
          'value': 'rlsynctet2',
        },
      },
      'SERVER_UNIQUE_POSITION': 'INVALID[]',
      'SERVER_VERSION': '1394241139528639',
      'SERVER_VERSION_TIME': '0',
      'SPECIFICS': {
        'autofill': {
          'name': 'Email',
          'usage_timestamp': ['13038713887000000', '13038713890000000'],
          'value': 'rlsynctet2',
        },
      },
      'SYNCING': false,
      'TRANSACTION_VERSION': '1',
      'UNIQUE_BOOKMARK_TAG': '',
      'UNIQUE_CLIENT_TAG': 'EvliorKUf1rLjT+BGkNZp586Tsk=',
      'UNIQUE_POSITION': 'INVALID[]',
      'UNIQUE_SERVER_TAG': '',
      'isDirty': false,
      'dataType': 'Autofill',
    },
  ],
  'type': 'Autofill',
}];

/**
 * A value to return in mock onReceivedUpdatedAboutInfo event.
 * @const
 */
const HARD_CODED_ABOUT_INFO = {
  'actionable_error': [
    {
      'stat_status': 'uninitialized',
      'stat_name': 'Error Type',
      'stat_value': 'Uninitialized',
    },
    {
      'stat_status': 'uninitialized',
      'stat_name': 'Action',
      'stat_value': 'Uninitialized',
    },
    {
      'stat_status': 'uninitialized',
      'stat_name': 'URL',
      'stat_value': 'Uninitialized',
    },
    {
      'stat_status': 'uninitialized',
      'stat_name': 'Error Description',
      'stat_value': 'Uninitialized',
    },
  ],
  'actionable_error_detected': false,
  'details': [
    {
      'data': [{
        'stat_status': '',
        'stat_name': 'Summary',
        'stat_value': 'Sync service initialized',
      }],
      'is_sensitive': false,
      'title': 'Summary',
    },
  ],
  'type_status': [
    {
      'status': 'header',
      'name': 'Data Type',
      'num_entries': 'Total Entries',
      'num_live': 'Live Entries',
      'message': 'Message',
      'state': 'State',
    },
    {
      'status': 'ok',
      'name': 'Bookmarks',
      'num_entries': 2793,
      'num_live': 2793,
      'message': '',
      'state': 'Running',
    },
  ],
  'unrecoverable_error_detected': false,
};

const NETWORK_EVENT_DETAILS_1 = {
  'details': 'Notified types: Bookmarks, Autofill',
  'proto': {},
  'time': 1395874542192.407,
  'type': 'Normal GetUpdate request',
};

const NETWORK_EVENT_DETAILS_2 = {
  'details': 'Received error: SYNC_AUTH_ERROR',
  'proto': {},
  'time': 1395874542192.837,
  'type': 'GetUpdates Response',
};

suite('SyncInternals', function() {
  test('Uninitialized', function() {
    assertNotEquals(null, getAboutInfoForTest());
  });

  // <if expr="is_chromeos">
  // Sync should be disabled if there was no primary account set.
  test('SyncDisabledByDefaultChromeOS', function() {
    assertTrue(hasInDetails(true, 'Transport State', 'Disabled'));
    // We don't check 'Disable Reasons' here because the string depends on the
    // flag SplitSettingsSync. There's not a good way to check a C++ flag value
    // in the middle of a JS test, nor is there a simple way to enable or
    // disable platform-specific flags in a cross-platform JS test suite.
    // TODO(crbug.com/1087165): When SplitSettingsSync is the default, delete
    // this test and use SyncInternalsWebUITest.SyncDisabledByDefault on all
    // platforms.
    assertTrue(hasInDetails(true, 'Username', ''));
  });
  // </if>

  // <if expr="not is_chromeos">
  // On non-ChromeOS, sync should be disabled if there was no primary account
  // set.
  test('SyncDisabledByDefault', function() {
    assertTrue(hasInDetails(true, 'Transport State', 'Disabled'));
    assertTrue(hasInDetails(true, 'Disable Reasons', 'Not signed in'));
    assertTrue(hasInDetails(true, 'Username', ''));
  });
  // </if>

  test('LoadPastedAboutInfo', function() {
    // Expose the text field.
    getRequiredElement('import-status').click();

    // Fill it with fake data.
    getRequiredElement<HTMLTextAreaElement>('status-text').value =
        JSON.stringify(HARD_CODED_ABOUT_INFO);

    // Trigger the import.
    getRequiredElement('import-status').click();

    assertTrue(hasInDetails(true, 'Summary', 'Sync service initialized'));
  });

  test('NetworkEventsTest', function() {
    webUIListenerCallback('onProtocolEvent', NETWORK_EVENT_DETAILS_1);
    webUIListenerCallback('onProtocolEvent', NETWORK_EVENT_DETAILS_2);

    // Make sure that both events arrived.
    const eventCount =
        getRequiredElement('traffic-event-container').children.length;
    assertGE(eventCount, 2);

    // Check that the event details are displayed.
    const displayedEvent1 =
        getRequiredElement('traffic-event-container').children[eventCount - 2]!;
    const displayedEvent2 =
        getRequiredElement('traffic-event-container').children[eventCount - 1]!;
    assertTrue(
        displayedEvent1.innerHTML.includes(NETWORK_EVENT_DETAILS_1.details));
    assertTrue(
        displayedEvent1.innerHTML.includes(NETWORK_EVENT_DETAILS_1.type));
    assertTrue(
        displayedEvent2.innerHTML.includes(NETWORK_EVENT_DETAILS_2.details));
    assertTrue(
        displayedEvent2.innerHTML.includes(NETWORK_EVENT_DETAILS_2.type));

    // Test that repeated events are not re-displayed.
    webUIListenerCallback('onProtocolEvent', NETWORK_EVENT_DETAILS_1);
    assertEquals(
        eventCount,
        getRequiredElement('traffic-event-container').children.length);
  });


  test('SearchTabDoesntChangeOnItemSelect', function() {
    // Select the search tab.
    const searchTab = getRequiredElement('sync-search-tab');
    const tabs = Array.from(document.querySelectorAll('div[slot=\'tab\']'));
    const index = tabs.indexOf(searchTab);
    getRequiredElement('sync-page')
        .setAttribute('selected-index', index.toString());
    assertTrue(searchTab.hasAttribute('selected'));

    // Build the data model and attach to result list.
    setupSyncResultsListForTest([
      {
        value: 'value 0',
        toString: function() {
          return 'node 0';
        },
      },
      {
        value: 'value 1',
        toString: function() {
          return 'node 1';
        },
      },
    ]);

    // Select the first list item and verify the search tab remains selected.
    const firstItem =
        getRequiredElement('sync-results-list').querySelector('li');
    assertTrue(!!firstItem);
    assertFalse(firstItem.hasAttribute('selected'));
    firstItem.click();
    // Verify that this selected the item.
    assertTrue(firstItem.hasAttribute('selected'));
    assertTrue(searchTab.hasAttribute('selected'));
  });

  test('NodeBrowserTest', function() {
    setAllNodesForTest(HARD_CODED_ALL_NODES);

    // Hit the refresh button.
    getRequiredElement('node-browser-refresh-button').click();

    // Check that the refresh time was updated.
    assertNotEquals(
        getRequiredElement('node-browser-refresh-time').textContent, 'Never');

    // Verify some hard-coded assumptions.  These depend on the value of the
    // hard-coded nodes, specified elsewhere in this file.

    // Start with the tree itself.
    const tree = getRequiredElement<CrTreeElement>('sync-node-tree');
    assertEquals(1, tree.items.length);

    // Check the type root and expand it.
    const typeRoot = tree.items[0]!;
    assertFalse(typeRoot.hasAttribute('expanded'));
    typeRoot.toggleAttribute('expanded', true);
    assertEquals(1, typeRoot.items.length);

    // An actual sync node.  The child of the type root.
    const leaf = typeRoot.items[0];
    assertTrue(!!leaf);

    // Verify that selecting it affects the details view.
    assertTrue(getRequiredElement('node-details').hasAttribute('hidden'));
    tree.selectedItem = leaf;
    assertTrue(leaf.hasAttribute('selected'));
    assertFalse(getRequiredElement('node-details').hasAttribute('hidden'));
  });

  test('NodeBrowserRefreshOnTabSelect', function() {
    setAllNodesForTest(HARD_CODED_ALL_NODES);

    // Should start with non-refreshed node browser.
    assertEquals(
        getRequiredElement('node-browser-refresh-time').textContent, 'Never');

    // Selecting the tab will refresh it.
    const syncBrowserTab = getRequiredElement('sync-browser-tab');
    const tabs = Array.from(document.querySelectorAll('div[slot=\'tab\']'));
    const index = tabs.indexOf(syncBrowserTab);
    const tabBox = getRequiredElement('sync-page');
    tabBox.setAttribute('selected-index', index.toString());
    assertTrue(syncBrowserTab.hasAttribute('selected'));
    assertNotEquals(
        getRequiredElement('node-browser-refresh-time').textContent, 'Never');

    // Re-selecting the tab shouldn't re-refresh.
    getRequiredElement('node-browser-refresh-time').textContent = 'TestCanary';
    tabBox.setAttribute('selected-index', '0');
    tabBox.setAttribute('selected-index', index.toString());
    assertEquals(
        getRequiredElement('node-browser-refresh-time').textContent,
        'TestCanary');
  });

  test('DumpSyncEventsToText', function() {
    // Dispatch an event.
    webUIListenerCallback('onProtocolEvent', {someField: 'someData'});

    // Click the dump-to-text button.
    getRequiredElement('dump-to-text').click();

    // Verify our event is among the results.
    const eventDumpText = getRequiredElement('data-dump').textContent!;

    assertGE(eventDumpText.indexOf('onProtocolEvent'), 0);
    assertGE(eventDumpText.indexOf('someData'), 0);
  });
});
