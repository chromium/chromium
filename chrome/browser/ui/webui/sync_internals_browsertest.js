// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "components/sync/driver/sync_driver_switches.h"');

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
   * Disable accessibility testing for this page.
   * @override
   */
  runAccessibilityChecks: false,

  /** @override */
  preLoad: function() {
    this.makeAndRegisterMockHandler([
        'getAllNodes',
    ]);
  },

  /**
   * Checks aboutInfo's details section for the specified field.
   * @param {boolean} isValid Whether the field is valid.
   * @param {string} key The name of the key to search for in details.
   * @param {string} value The expected value if |key| is found.
   * @return {boolean} whether the field was found in the details.
   * @protected
   */
  hasInDetails: function(isValid, key, value) {
    var details = chrome.sync.aboutInfo.details;
    if (!details) {
      return false;
    }
    for (var i = 0; i < details.length; ++i) {
      if (!details[i].data) {
        continue;
      }
      for (var j = 0; j < details[i].data.length; ++j) {
        var obj = details[i].data[j];
        if (obj.stat_name == key) {
          return obj.is_valid == isValid && obj.stat_value == value;
        }
      }
    }
    return false;
  }
};

/**
 * Constant hard-coded value to return from mock getAllNodes.
 * @const
 */
var HARD_CODED_ALL_NODES = [{
  'nodes': [{
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
    'SERVER_SPECIFICS': {
      'autofill': {
        'usage_timestamp': []
      }
    },
    'SERVER_UNIQUE_POSITION': 'INVALID[]',
    'SERVER_VERSION': '1396470970810000',
    'SPECIFICS': {
      'autofill': {
        'usage_timestamp': []
      }
    },
    'SYNCING': false,
    'TRANSACTION_VERSION': '1',
    'UNIQUE_BOOKMARK_TAG': '',
    'UNIQUE_CLIENT_TAG': '',
    'UNIQUE_POSITION': 'INVALID[]',
    'UNIQUE_SERVER_TAG': 'google_chrome_autofill',
    'isDirty': false,
    'modelType': 'Autofill'
  }, {
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
        'value': 'rlsynctet2'
      }
    },
    'SERVER_UNIQUE_POSITION': 'INVALID[]',
    'SERVER_VERSION': '1394241139528639',
    'SPECIFICS': {
      'autofill': {
        'name': 'Email',
        'usage_timestamp': ['13038713887000000', '13038713890000000'],
        'value': 'rlsynctet2'
      }
    },
    'SYNCING': false,
    'TRANSACTION_VERSION': '1',
    'UNIQUE_BOOKMARK_TAG': '',
    'UNIQUE_CLIENT_TAG': 'EvliorKUf1rLjT+BGkNZp586Tsk=',
    'UNIQUE_POSITION': 'INVALID[]',
    'UNIQUE_SERVER_TAG': '',
    'isDirty': false,
    'modelType': 'Autofill'
  }],
  'type': 'Autofill'
}];

/**
 * A value to return in mock onReceivedUpdatedAboutInfo event.
 * @const
 */
HARD_CODED_ABOUT_INFO = {
  'actionable_error': [
    {
      'is_valid': false,
      'stat_name': 'Error Type',
      'stat_value': 'Uninitialized'
    },
    {
      'is_valid': false,
      'stat_name': 'Action',
      'stat_value': 'Uninitialized'
    },
    {
      'is_valid': false,
      'stat_name': 'URL',
      'stat_value': 'Uninitialized'
    },
    {
      'is_valid': false,
      'stat_name': 'Error Description',
      'stat_value': 'Uninitialized'
    }
  ],
  'actionable_error_detected': false,
  'details': [
    {
      'data': [
        {
          'is_valid': true,
          'stat_name': 'Summary',
          'stat_value': 'Sync service initialized'
        }
      ],
      'is_sensitive': false,
      'title': 'Summary'
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
      'group_type': 'Group Type',
    },
    {
      'status': 'ok',
      'name': 'Bookmarks',
      'num_entries': 2793,
      'num_live': 2793,
      'message': '',
      'state': 'Running',
      'group_type': 'Group UI',
    },
  ],
  'unrecoverable_error_detected': false
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
   assertNotEquals(null, chrome.sync.aboutInfo);
});

GEN('#if defined(OS_CHROMEOS)');

// On ChromeOS, browser tests are signed in by default to mimic production,
// so the sync transport layer should be enabled. Note that the sync *feature*
// might still be disabled depending on how the test infrastructure is
// configured.
TEST_F('SyncInternalsWebUITest', 'SyncTransportEnabledByDefault', function() {
  // The specific transport state is dependent on the timing of startup, but it
  // should not be disabled.
  expectFalse(this.hasInDetails(true, 'Transport State', 'Disabled'));
});

GEN('#else');

// On non-ChromeOS, sync should be disabled if there was no primary account
// set.
TEST_F('SyncInternalsWebUITest', 'SyncDisabledByDefault', function() {
  expectTrue(this.hasInDetails(true, 'Transport State', 'Disabled'));
  expectTrue(
      this.hasInDetails(true, 'Disable Reasons', 'Not signed in, User choice'));
  expectTrue(this.hasInDetails(true, 'Username', ''));
});

GEN('#endif  // defined(OS_CHROMEOS)');

TEST_F('SyncInternalsWebUITest', 'LoadPastedAboutInfo', function() {
  // Expose the text field.
  $('import-status').click();

  // Fill it with fake data.
  $('status-text').value = JSON.stringify(HARD_CODED_ABOUT_INFO);

  // Trigger the import.
  $('import-status').click();

  expectTrue(this.hasInDetails(true, 'Summary', 'Sync service initialized'));
});

TEST_F('SyncInternalsWebUITest', 'NetworkEventsTest', function() {
  let networkEvent1 = new Event('onProtocolEvent');
  networkEvent1.details = NETWORK_EVENT_DETAILS_1;
  let networkEvent2 = new Event('onProtocolEvent');
  networkEvent2.details = NETWORK_EVENT_DETAILS_2;

  chrome.sync.events.dispatchEvent(networkEvent1);
  chrome.sync.events.dispatchEvent(networkEvent2);

  // Make sure that both events arrived.
  let eventCount = $('traffic-event-container').children.length;
  assertGE(eventCount, 2);

  // Check that the event details are displayed.
  let displayedEvent1 = $('traffic-event-container').children[eventCount - 2];
  let displayedEvent2 = $('traffic-event-container').children[eventCount - 1];
  expectTrue(
      displayedEvent1.innerHTML.includes(NETWORK_EVENT_DETAILS_1.details));
  expectTrue(displayedEvent1.innerHTML.includes(NETWORK_EVENT_DETAILS_1.type));
  expectTrue(
      displayedEvent2.innerHTML.includes(NETWORK_EVENT_DETAILS_2.details));
  expectTrue(displayedEvent2.innerHTML.includes(NETWORK_EVENT_DETAILS_2.type));

  // Test that repeated events are not re-displayed.
  chrome.sync.events.dispatchEvent(networkEvent1);
  expectEquals(eventCount, $('traffic-event-container').children.length);
});

TEST_F('SyncInternalsWebUITest', 'SearchTabDoesntChangeOnItemSelect',
       function() {
  // Select the search tab.
  $('sync-search-tab').selected = true;
  expectTrue($('sync-search-tab').selected);

  // Build the data model and attach to result list.
  cr.ui.List.decorate($('sync-results-list'));
  $('sync-results-list').dataModel = new cr.ui.ArrayDataModel([
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
    }
  ]);

  // Select the first list item and verify the search tab remains selected.
  $('sync-results-list').getListItemByIndex(0).selected = true;
  expectTrue($('sync-search-tab').selected);
});

TEST_F('SyncInternalsWebUITest', 'NodeBrowserTest', function() {
  var getAllNodesSavedArgs = new SaveMockArguments();
  this.mockHandler.expects(once()).
      getAllNodes(getAllNodesSavedArgs.match(ANYTHING)).
      will(callFunctionWithSavedArgs(getAllNodesSavedArgs,
                                     chrome.sync.getAllNodesCallback,
                                     HARD_CODED_ALL_NODES));

  // Hit the refresh button.
  $('node-browser-refresh-button').click();

  // Check that the refresh time was updated.
  expectNotEquals($('node-browser-refresh-time').textContent, 'Never');

  // Verify some hard-coded assumptions.  These depend on the vaue of the
  // hard-coded nodes, specified elsewhere in this file.

  // Start with the tree itself.
  var tree = $('sync-node-tree');
  assertEquals(1, tree.items.length);

  // Check the type root and expand it.
  var typeRoot = tree.items[0];
  expectFalse(typeRoot.expanded);
  typeRoot.expanded = true;
  assertEquals(1, typeRoot.items.length);

  // An actual sync node.  The child of the type root.
  var leaf = typeRoot.items[0];

  // Verify that selecting it affects the details view.
  expectTrue($('node-details').hasAttribute('hidden'));
  leaf.selected = true;
  expectFalse($('node-details').hasAttribute('hidden'));
});

TEST_F('SyncInternalsWebUITest', 'NodeBrowserRefreshOnTabSelect', function() {
  var getAllNodesSavedArgs = new SaveMockArguments();
  this.mockHandler.expects(once()).
      getAllNodes(getAllNodesSavedArgs.match(ANYTHING)).
      will(callFunctionWithSavedArgs(getAllNodesSavedArgs,
                                     chrome.sync.getAllNodesCallback,
                                     HARD_CODED_ALL_NODES));

  // Should start with non-refreshed node browser.
  expectEquals($('node-browser-refresh-time').textContent, 'Never');

  // Selecting the tab will refresh it.
  $('sync-browser-tab').selected = true;
  expectNotEquals($('node-browser-refresh-time').textContent, 'Never');

  // Re-selecting the tab shouldn't re-refresh.
  $('node-browser-refresh-time').textContent = 'TestCanary';
  $('sync-browser-tab').selected = false;
  $('sync-browser-tab').selected = true;
  expectEquals($('node-browser-refresh-time').textContent, 'TestCanary');
});

// Tests that the events log page correctly receives and displays an event.
TEST_F('SyncInternalsWebUITest', 'EventLogTest', function() {
  // Dispatch an event.
  var connectionEvent = new Event('onConnectionStatusChange');
  connectionEvent.details = {'status': 'CONNECTION_OK'};
  chrome.sync.events.dispatchEvent(connectionEvent);

  // Verify that it is displayed in the events log.
  var syncEventsTable = $('sync-events');
  assertGE(syncEventsTable.children.length, 1);
  var lastRow = syncEventsTable.children[syncEventsTable.children.length - 1];

  // Makes some assumptions about column ordering.  We'll need re-think this if
  // it turns out to be a maintenance burden.
  assertEquals(4, lastRow.children.length);
  var detailsText = lastRow.children[0].textContent;
  var submoduleName = lastRow.children[1].textContent;
  var eventName = lastRow.children[2].textContent;

  expectGE(submoduleName.indexOf('manager'), 0,
      'submoduleName=' + submoduleName);
  expectGE(eventName.indexOf('onConnectionStatusChange'), 0,
      'eventName=' + eventName);
  expectGE(detailsText.indexOf('CONNECTION_OK'), 0,
      'detailsText=' + detailsText);
});

TEST_F('SyncInternalsWebUITest', 'DumpSyncEventsToText', function() {
  // Dispatch an event.
  var connectionEvent = new Event('onConnectionStatusChange');
  connectionEvent.details = {'status': 'CONNECTION_OK'};
  chrome.sync.events.dispatchEvent(connectionEvent);

  // Click the dump-to-text button.
  $('dump-to-text').click();

  // Verify our event is among the results.
  var eventDumpText = $('data-dump').textContent;

  expectGE(eventDumpText.indexOf('onConnectionStatusChange'), 0);
  expectGE(eventDumpText.indexOf('CONNECTION_OK'), 0);
});
