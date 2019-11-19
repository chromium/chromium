// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestStore} from 'chrome://test/bookmarks/test_store.js';
import {selectItem} from 'chrome://bookmarks/bookmarks.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from 'chrome://test/bookmarks/test_util.js';

suite('<bookmarks-item>', function() {
  let item;
  let store;
  const TEST_ITEM = createItem('0');

  setup(function() {
    const nodes = testTree(createFolder('1', [
      createItem('2', {url: 'http://example.com/'}),
      createItem('3'),
    ]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
    });
    store.replaceSingleton();

    item = document.createElement('bookmarks-item');
    item.itemId = '2';
    replaceBody(item);
  });

  test('changing the url changes the favicon', function() {
    const favicon = item.$.icon.style.backgroundImage;
    store.data.nodes['2'] = createItem('0', {url: 'https://mail.google.com'});
    store.notifyObservers();
    assertNotEquals(favicon, item.$.icon.style.backgroundImage);
  });

  test('changing to folder hides/unhides the folder/icon', function() {
    // Starts test as an item.
    assertEquals('website-icon', item.$.icon.className);

    // Change to a folder.
    item.itemId = '1';

    assertEquals('folder-icon', item.$.icon.className);
  });

  test('pressing the menu button selects the item', function() {
    item.$.menuButton.click();
    assertDeepEquals(
        selectItem('2', store.data, {
          clear: true,
          range: false,
          toggle: false,
        }),
        store.lastAction);
  });

  function testEventSelection(eventname) {
    item.isSelectedItem_ = true;
    item.dispatchEvent(new MouseEvent(eventname));
    assertEquals(null, store.lastAction);

    item.isSelectedItem_ = false;
    item.dispatchEvent(new MouseEvent(eventname));
    assertDeepEquals(
        selectItem('2', store.data, {
          clear: true,
          range: false,
          toggle: false,
        }),
        store.lastAction);
  }

  test('context menu selects item if unselected', function() {
    testEventSelection('contextmenu');
  });

  test('doubleclicking selects item if unselected', function() {
    document.body.appendChild(
        document.createElement('bookmarks-command-manager'));
    testEventSelection('dblclick');
  });
});
