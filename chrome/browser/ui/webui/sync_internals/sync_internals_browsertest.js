// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "build/build_config.h"');
GEN('#include "components/sync/base/features.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for sync internals WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function SyncInternalsWebUITest() {}

SyncInternalsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the sync internals page.
   * @override
   */
  browsePreload: 'chrome://sync-internals',

  /**
   * Checks aboutInfo's details section for the specified field.
   * @param {boolean} isValid Whether the field is valid.
   * @param {string} key The name of the key to search for in details.
   * @param {string} value The expected value if |key| is found.
   * @return {boolean} whether the field was found in the details.
   * @protected
   */
  hasInDetails: function(isValid, key, value) {
    const details = getAboutInfoForTest().details;
    if (!details) {
      return false;
    }
    for (let i = 0; i < details.length; ++i) {
      if (!details[i].data) {
        continue;
      }
      for (let j = 0; j < details[i].data.length; ++j) {
        const obj = details[i].data[j];
        if (obj.stat_name === key) {
          return (obj.stat_status !== 'uninitialized') === isValid &&
              obj.stat_value === value;
        }
      }
    }
    return false;
  },
};

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
      'META_HANDLE': '387',
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
      'SPECIFICS': {'autofill': {'usage_timestamp': []}},
      'SYNCING': false,
      'TRANSACTION_VERSION': '1',
      'UNIQUE_BOOKMARK_TAG': '',
      'UNIQUE_CLIENT_TAG': '',
      'UNIQUE_POSITION': 'INVALID[]',
      'UNIQUE_SERVER_TAG': 'google_chrome_autofill',
      'isDirty': false,
      'modelType': 'Autofill',
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
      'META_HANDLE': '2989',
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
      'modelType': 'Autofill',
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
      'name': 'Model Type',
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

NETWORK_EVENT_DETAILS_1 = {
  'details': 'Notified types: Bookmarks, Autofill',
  'proto': {},
  'time': 1395874542192.407,
  'type': 'Normal GetUpdate request',
};

NETWORK_EVENT_DETAILS_2 = {
  'details': 'Received error: SYNC_AUTH_ERROR',
  'proto': {},
  'time': 1395874542192.837,
  'type': 'GetUpdates Response',
};

TEST_F('SyncInternalsWebUITest', 'Uninitialized', function() {
  assertNotEquals(null, getAboutInfoForTest());
});

GEN('#if BUILDFLAG(IS_CHROMEOS)');

// Sync should be disabled if there was no primary account set.
TEST_F('SyncInternalsWebUITest', 'SyncDisabledByDefaultChromeOS', function() {
  assertTrue(this.hasInDetails(true, 'Transport State', 'Disabled'));
  // We don't check 'Disable Reasons' here because the string depends on the
  // flag SplitSettingsSync. There's not a good way to check a C++ flag value
  // in the middle of a JS test, nor is there a simple way to enable or disable
  // platform-specific flags in a cross-platform JS test suite.
  // TODO(crbug.com/1087165): When SplitSettingsSync is the default, delete this
  // test and use SyncInternalsWebUITest.SyncDisabledByDefault on all
  // platforms.
  assertTrue(this.hasInDetails(true, 'Username', ''));
});

GEN('#else');

// On non-ChromeOS, sync should be disabled if there was no primary account set.
TEST_F('SyncInternalsWebUITest', 'SyncDisabledByDefault', function() {
  assertTrue(this.hasInDetails(true, 'Transport State', 'Disabled'));
  assertTrue(
      this.hasInDetails(true, 'Disable Reasons', 'Not signed in'));
  assertTrue(this.hasInDetails(true, 'Username', ''));
});

GEN('#endif');

TEST_F('SyncInternalsWebUITest', 'LoadPastedAboutInfo', function() {
  // Expose the text field.
  document.querySelector('#import-status').click();

  // Fill it with fake data.
  document.querySelector('#status-text').value =
      JSON.stringify(HARD_CODED_ABOUT_INFO);

  // Trigger the import.
  document.querySelector('#import-status').click();

  assertTrue(this.hasInDetails(true, 'Summary', 'Sync service initialized'));
});

TEST_F('SyncInternalsWebUITest', 'NetworkEventsTest', function() {
  cr.webUIListenerCallback('onProtocolEvent', NETWORK_EVENT_DETAILS_1);
  cr.webUIListenerCallback('onProtocolEvent', NETWORK_EVENT_DETAILS_2);

  // Make sure that both events arrived.
  const eventCount =
      document.querySelector('#traffic-event-container').children.length;
  assertGE(eventCount, 2);

  // Check that the event details are displayed.
  const displayedEvent1 = document.querySelector('#traffic-event-container')
                              .children[eventCount - 2];
  const displayedEvent2 = document.querySelector('#traffic-event-container')
                              .children[eventCount - 1];
  assertTrue(
      displayedEvent1.innerHTML.includes(NETWORK_EVENT_DETAILS_1.details));
  assertTrue(displayedEvent1.innerHTML.includes(NETWORK_EVENT_DETAILS_1.type));
  assertTrue(
      displayedEvent2.innerHTML.includes(NETWORK_EVENT_DETAILS_2.details));
  assertTrue(displayedEvent2.innerHTML.includes(NETWORK_EVENT_DETAILS_2.type));

  // Test that repeated events are not re-displayed.
  cr.webUIListenerCallback('onProtocolEvent', NETWORK_EVENT_DETAILS_1);
  assertEquals(
      eventCount,
      document.querySelector('#traffic-event-container').children.length);
});

TEST_F('SyncInternalsWebUITest', 'SearchTabDoesntChangeOnItemSelect',
       function() {
  // Select the search tab.
  const searchTab = document.querySelector('#sync-search-tab');
  const tabs = Array.from(document.querySelectorAll('div[slot=\'tab\']'));
  const index = tabs.indexOf(searchTab);
  document.querySelector('cr-tab-box').setAttribute(
      'selected-index', index.toString());
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
      document.querySelector('#sync-results-list').querySelector('li');
  assertFalse(firstItem.hasAttribute('selected'));
  firstItem.click();
  // Verify that this selected the item.
  assertTrue(firstItem.hasAttribute('selected'));
  assertTrue(searchTab.hasAttribute('selected'));
});

TEST_F('SyncInternalsWebUITest', 'NodeBrowserTest', function() {
  setAllNodesForTest(HARD_CODED_ALL_NODES);

  // Hit the refresh button.
  document.querySelector('#node-browser-refresh-button').click();

  // Check that the refresh time was updated.
  assertNotEquals(
      document.querySelector('#node-browser-refresh-time').textContent,
      'Never');

  // Verify some hard-coded assumptions.  These depend on the value of the
  // hard-coded nodes, specified elsewhere in this file.

  // Start with the tree itself.
  const tree = document.querySelector('#sync-node-tree');
  assertEquals(1, tree.items.length);

  // Check the type root and expand it.
  const typeRoot = tree.items[0];
  assertFalse(typeRoot.hasAttribute('expanded'));
  typeRoot.toggleAttribute('expanded', true);
  assertEquals(1, typeRoot.items.length);

  // An actual sync node.  The child of the type root.
  const leaf = typeRoot.items[0];

  // Verify that selecting it affects the details view.
  assertTrue(document.querySelector('#node-details').hasAttribute('hidden'));
  tree.selectedItem = leaf;
  assertTrue(leaf.hasAttribute('selected'));
  assertFalse(document.querySelector('#node-details').hasAttribute('hidden'));
});

TEST_F('SyncInternalsWebUITest', 'NodeBrowserRefreshOnTabSelect', function() {
  setAllNodesForTest(HARD_CODED_ALL_NODES);

  // Should start with non-refreshed node browser.
  assertEquals(
      document.querySelector('#node-browser-refresh-time').textContent,
      'Never');

  // Selecting the tab will refresh it.
  const syncBrowserTab = document.querySelector('#sync-browser-tab');
  const tabs = Array.from(document.querySelectorAll('div[slot=\'tab\']'));
  const index = tabs.indexOf(syncBrowserTab);
  document.querySelector('cr-tab-box').setAttribute(
      'selected-index', index.toString());
  assertTrue(syncBrowserTab.hasAttribute('selected'));
  assertNotEquals(
      document.querySelector('#node-browser-refresh-time').textContent,
      'Never');

  // Re-selecting the tab shouldn't re-refresh.
  document.querySelector('#node-browser-refresh-time').textContent =
      'TestCanary';
  document.querySelector('cr-tab-box').setAttribute('selected-index', '0');
  document.querySelector('cr-tab-box').setAttribute(
      'selected-index', index.toString());
  assertEquals(
      document.querySelector('#node-browser-refresh-time').textContent,
      'TestCanary');
});

TEST_F('SyncInternalsWebUITest', 'DumpSyncEventsToText', function() {
  // Dispatch an event.
  cr.webUIListenerCallback('onProtocolEvent', {someField: 'someData'});

  // Click the dump-to-text button.
  document.querySelector('#dump-to-text').click();

  // Verify our event is among the results.
  const eventDumpText = document.querySelector('#data-dump').textContent;

  assertGE(eventDumpText.indexOf('onProtocolEvent'), 0);
  assertGE(eventDumpText.indexOf('someData'), 0);
});
