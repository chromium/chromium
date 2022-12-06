// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-item-list. */
import 'chrome://extensions/extensions.js';

import {ExtensionsItemListElement} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {createExtensionInfo, testVisible} from './test_util.js';

const extension_item_list_tests = {
  suiteName: 'ExtensionItemListTest',
  TestNames: {
    Filtering: 'item list filtering',
    NoItemsMsg: 'empty item list',
    NoSearchResultsMsg: 'empty item list filtering results',
    LoadTimeData: 'loadTimeData contains isManaged and managedByOrg',
  },
};

Object.assign(window, {extension_item_list_tests: extension_item_list_tests});

suite(extension_item_list_tests.suiteName, function() {
  let itemList: ExtensionsItemListElement;
  let boundTestVisible: (selector: string, visible: boolean, text?: string) =>
      void;

  // Initialize an extension item before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    itemList = document.createElement('extensions-item-list');
    boundTestVisible = testVisible.bind(null, itemList);

    const createExt = createExtensionInfo;
    const extensionItems = [
      createExt({name: 'Alpha', id: 'a'.repeat(32)}),
      createExt({name: 'Bravo', id: 'b'.repeat(32)}),
      createExt({name: 'Charlie', id: 'c'.repeat(29) + 'wxy'}),
    ];
    const appItems = [
      createExt({name: 'QQ', id: 'q'.repeat(32)}),
    ];
    itemList.extensions = extensionItems;
    itemList.apps = appItems;
    itemList.filter = '';
    document.body.appendChild(itemList);
  });

  test(extension_item_list_tests.TestNames.Filtering, function() {
    function itemLengthEquals(num: number) {
      flush();
      assertEquals(
          itemList.shadowRoot!.querySelectorAll('extensions-item').length, num);
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
    assertEquals(
        'Bravo',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
    // Test inner substring (rather than prefix).
    itemList.filter = 'lph';
    itemLengthEquals(1);
    assertEquals(
        'Alpha',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
    // Test trailing/leading spaces.
    itemList.filter = '   Alpha  ';
    itemLengthEquals(1);
    assertEquals(
        'Alpha',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
    // Test string with no matching items.
    itemList.filter = 'z';
    itemLengthEquals(0);
    // A filter of '' should reset to show all items.
    itemList.filter = '';
    itemLengthEquals(4);
    // A filter of 'q' should should show just the apps item.
    itemList.filter = 'q';
    itemLengthEquals(1);
    // A filter of 'xy' should show just the 'Charlie' item since its id
    // matches.
    itemList.filter = 'xy';
    itemLengthEquals(1);
    assertEquals(
        'Charlie',
        itemList.shadowRoot!.querySelector('extensions-item')!.data.name);
  });

  test(extension_item_list_tests.TestNames.NoItemsMsg, function() {
    flush();
    boundTestVisible('#no-items', false);
    boundTestVisible('#no-search-results', false);

    itemList.extensions = [];
    itemList.apps = [];
    flush();
    boundTestVisible('#no-items', true);
    boundTestVisible('#no-search-results', false);
  });

  test(extension_item_list_tests.TestNames.NoSearchResultsMsg, function() {
    flush();
    boundTestVisible('#no-items', false);
    boundTestVisible('#no-search-results', false);

    itemList.filter = 'non-existent name';
    flush();
    boundTestVisible('#no-items', false);
    boundTestVisible('#no-search-results', true);
  });

  test(extension_item_list_tests.TestNames.LoadTimeData, function() {
    // Check that loadTimeData contains these values.
    loadTimeData.getBoolean('isManaged');
    loadTimeData.getString('browserManagedByOrg');
  });
});
