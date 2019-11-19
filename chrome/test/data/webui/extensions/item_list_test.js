// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-item-list. */
import 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createExtensionInfo, testVisible} from './test_util.js';

window.extension_item_list_tests = {};
extension_item_list_tests.suiteName = 'ExtensionItemListTest';
/** @enum {string} */
extension_item_list_tests.TestNames = {
  Filtering: 'item list filtering',
  NoItemsMsg: 'empty item list',
  NoSearchResultsMsg: 'empty item list filtering results',
  LoadTimeData: 'loadTimeData contains isManaged and managedByOrg',
};

suite(extension_item_list_tests.suiteName, function() {
  /** @type {extensions.ItemList} */
  let itemList;
  let boundTestVisible;

  // Initialize an extension item before each test.
  setup(function() {
    PolymerTest.clearBody();
    itemList = document.createElement('extensions-item-list');
    boundTestVisible = testVisible.bind(null, itemList);

    const createExt = createExtensionInfo;
    const extensionItems = [
      createExt({name: 'Alpha', id: 'a'.repeat(32)}),
      createExt({name: 'Bravo', id: 'b'.repeat(32)}),
      createExt({name: 'Charlie', id: 'c'.repeat(32)})
    ];
    const appItems = [
      createExt({name: 'QQ', id: 'q'.repeat(32)}),
    ];
    itemList.extensions = extensionItems;
    itemList.apps = appItems;
    itemList.filter = '';
    document.body.appendChild(itemList);
  });

  test(assert(extension_item_list_tests.TestNames.Filtering), function() {
    function itemLengthEquals(num) {
      flush();
      expectEquals(
          itemList.shadowRoot.querySelectorAll('extensions-item').length, num);
    }

    // We should initially show all the items.
    itemLengthEquals(4);

    // All extension items have an 'a'.
    itemList.filter = 'a';
    itemLengthEquals(3);
    // Filtering is case-insensitive, so all extension items should be shown.
    itemList.filter = 'A';
    itemLengthEquals(3);
    // Only 'Bravo' has a 'b'.
    itemList.filter = 'b';
    itemLengthEquals(1);
    expectEquals('Bravo', itemList.$$('extensions-item').data.name);
    // Test inner substring (rather than prefix).
    itemList.filter = 'lph';
    itemLengthEquals(1);
    expectEquals('Alpha', itemList.$$('extensions-item').data.name);
    // Test trailing/leading spaces.
    itemList.filter = '   Alpha  ';
    itemLengthEquals(1);
    expectEquals('Alpha', itemList.$$('extensions-item').data.name);
    // Test string with no matching items.
    itemList.filter = 'z';
    itemLengthEquals(0);
    // A filter of '' should reset to show all items.
    itemList.filter = '';
    itemLengthEquals(4);
    // A filter of 'q' should should show just the apps item.
    itemList.filter = 'q';
    itemLengthEquals(1);
  });

  test(assert(extension_item_list_tests.TestNames.NoItemsMsg), function() {
    flush();
    boundTestVisible('#no-items', false);
    boundTestVisible('#no-search-results', false);

    itemList.extensions = [];
    itemList.apps = [];
    flush();
    boundTestVisible('#no-items', true);
    boundTestVisible('#no-search-results', false);
  });

  test(
      assert(extension_item_list_tests.TestNames.NoSearchResultsMsg),
      function() {
        flush();
        boundTestVisible('#no-items', false);
        boundTestVisible('#no-search-results', false);

        itemList.filter = 'non-existent name';
        flush();
        boundTestVisible('#no-items', false);
        boundTestVisible('#no-search-results', true);
      });

  test(assert(extension_item_list_tests.TestNames.LoadTimeData), function() {
    // Check that loadTimeData contains these values.
    loadTimeData.getBoolean('isManaged');
    loadTimeData.getString('browserManagedByOrg');
  });
});
