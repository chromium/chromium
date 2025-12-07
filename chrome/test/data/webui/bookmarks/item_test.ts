// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarkNode, BookmarksItemElement} from 'chrome://bookmarks/bookmarks.js';
import {BrowserProxyImpl, selectItem} from 'chrome://bookmarks/bookmarks.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarksBrowserProxy} from './test_browser_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-item>', function() {
  let item: BookmarksItemElement;
  let store: TestStore;
  let testBrowserProxy: TestBookmarksBrowserProxy;

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

    testBrowserProxy = new TestBookmarksBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    item = document.createElement('bookmarks-item');
    item.itemId = '2';
    replaceBody(item);
  });

  test('changing the url changes the favicon', async () => {
    const favicon = item.$.icon.style.backgroundImage;
    store.data.nodes['2'] =
        (createItem('0', {url: 'https://mail.google.com'}) as unknown as
         BookmarkNode);
    store.notifyObservers();
    await microtasksFinished();
    assertNotEquals(favicon, item.$.icon.style.backgroundImage);
  });

  test('changing to folder hides/unhides the folder/icon', async () => {
    // Starts test as an item.
    assertEquals('website-icon', item.$.icon.className);

    // Change to a folder.
    item.itemId = '1';
    await microtasksFinished();

    assertEquals('folder-icon icon-folder-open', item.$.icon.className);
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

  function testEventSelection(eventname: string) {
    item.setIsSelectedItemForTesting(true);
    item.dispatchEvent(new MouseEvent(eventname));
    assertEquals(null, store.lastAction);

    item.setIsSelectedItemForTesting(false);
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

  // TODO(crbug.com/413637076): Add tests that icon is only visible when the
  // bookmark is eligible for upload.
  test('cloud icon for upload to account storage is visible', async function() {
    assertFalse(isChildVisible(item, '#account-upload-button'));

    testBrowserProxy.setCanUploadAsAccountBookmark(true);
    testBrowserProxy.resetResolver('getCanUploadBookmarkToAccountStorage');

    // This triggers an update of the icon's visibility.
    item.itemId = '3';

    const [idRequested] = await testBrowserProxy.whenCalled(
        'getCanUploadBookmarkToAccountStorage');
    assertEquals('3', idRequested);
    await microtasksFinished();

    assertTrue(isChildVisible(item, '#account-upload-button'));

    testBrowserProxy.setCanUploadAsAccountBookmark(false);
    testBrowserProxy.resetResolver('getCanUploadBookmarkToAccountStorage');

    // Notify that bookmarks sync state has been updated. The icon's visibility
    // should be updated accordingly.
    webUIListenerCallback('bookmarks-sync-state-changed');

    await microtasksFinished();
    assertFalse(isChildVisible(item, '#account-upload-button'));
  });

  test(
      'cloud icon for upload to account storage click forwards call',
      async function() {
        // Show the cloud upload icon.
        testBrowserProxy.setCanUploadAsAccountBookmark(true);
        item.itemId = '3';
        await microtasksFinished();
        assertTrue(isChildVisible(item, '#account-upload-button'));

        // Click on the upload icon.
        const uploadIcon = item.shadowRoot.querySelector<HTMLElement>(
            '#account-upload-button');
        assertTrue(!!uploadIcon);
        uploadIcon.click();

        // The call should be forwarded with the correct id.
        const [idRequested] =
            await testBrowserProxy.whenCalled('onSingleBookmarkUploadClicked');
        assertEquals('3', idRequested);
      });
});
