// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarksApiProxyImpl, BookmarksAppElement, BookmarksItemElement, HIDE_FOCUS_RING_ATTRIBUTE, LOCAL_STORAGE_FOLDER_STATE_KEY, LOCAL_STORAGE_TREE_WIDTH_KEY} from 'chrome://bookmarks/bookmarks.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {down, keyDownOn, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertDeepEquals, assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, normalizeIterable, replaceBody} from './test_util.js';

suite('<bookmarks-app>', function() {
  let app: BookmarksAppElement;
  let store: TestStore;
  let testBookmarksApiProxy: TestBookmarksApiProxy;

  function resetStore() {
    store = new TestStore({});
    store.acceptInitOnce();
    store.replaceSingleton();
    testBookmarksApiProxy.setGetTree([
      createFolder(
          '0',
          [
            createFolder(
                '1',
                [
                  createFolder('11', []),
                ]),
          ]),
    ]);
  }

  setup(function() {
    window.localStorage.clear();
    testBookmarksApiProxy = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(testBookmarksApiProxy);
    resetStore();

    app = document.createElement('bookmarks-app');
    replaceBody(app);
    return flushTasks();
  });

  test('write and load closed folder state', async function() {
    const folderOpenStateList = [['1', true] as const];
    const folderOpenState = new Map(folderOpenStateList);
    store.data.folderOpenState = folderOpenState;
    store.notifyObservers();

    // Ensure closed folders are written to local storage.
    assertDeepEquals(
        JSON.stringify(Array.from(folderOpenState)),
        window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY]);

    resetStore();
    app = document.createElement('bookmarks-app');
    replaceBody(app);
    await flushTasks();

    // Ensure closed folders are read from local storage.
    assertDeepEquals(
        folderOpenStateList, normalizeIterable(store.data.folderOpenState));
  });

  test('write and load sidebar width', async function() {
    assertEquals(
        getComputedStyle(app.$.sidebar).width,
        app.shadowRoot!.querySelector('bookmarks-toolbar')!.sidebarWidth);

    const sidebarWidth = '500px';
    app.$.sidebar.style.width = sidebarWidth;
    app.$.splitter.dispatchEvent(new CustomEvent('resize'));
    assertEquals(
        sidebarWidth, window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY]);

    app = document.createElement('bookmarks-app');
    replaceBody(app);
    await flushTasks();

    assertEquals(sidebarWidth, app.$.sidebar.style.width);
  });

  test('focus ring hides and restores', async function() {
    const list = app.shadowRoot!.querySelector('bookmarks-list');
    await flushTasks();
    const item = list!.root!.querySelectorAll('bookmarks-item')[0] as
        BookmarksItemElement;
    const getFocusAttribute = () => app.getAttribute(HIDE_FOCUS_RING_ATTRIBUTE);

    assertEquals(null, getFocusAttribute());

    down(item);
    assertEquals('', getFocusAttribute());

    keyDownOn(item, 16, [], 'Shift');
    assertEquals('', getFocusAttribute());

    // This event is also captured by the bookmarks-list and propagation is
    // stopped. Regardless, it should clear the focus first.
    keyDownOn(item, 40, [], 'ArrowDown');
    assertEquals(null, getFocusAttribute());
  });

  test('when find shortcut is invoked, focus on search input', () => {
    const searchInput =
        app.shadowRoot!.querySelector(
                           'bookmarks-toolbar')!.searchField.getSearchInput();
    assertNotEquals(searchInput, getDeepActiveElement());
    pressAndReleaseKeyOn(document.body, 0, isMac ? 'meta' : 'ctrl', 'f');
    assertEquals(searchInput, getDeepActiveElement());
  });
});
