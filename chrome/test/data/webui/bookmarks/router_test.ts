// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SelectFolderAction, StartSearchAction} from 'chrome://bookmarks/bookmarks.js';
import {BookmarksApiProxyImpl, CrRouter, getDisplayedList, Store} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-router>', function() {
  let store: TestStore;

  function navigateTo(route: string) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('popstate'));
  }

  setup(function() {
    const nodes = testTree(createFolder('1', [createFolder('2', [])]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
      search: {
        term: '',
      },
    });
    store.replaceSingleton();

    const router = document.createElement('bookmarks-router');
    replaceBody(router);
    return flushTasks();
  });

  test('search updates from route', function() {
    navigateTo('/?q=bleep');
    const action = store.lastAction as StartSearchAction;
    assertEquals('start-search', action.name);
    assertEquals('bleep', action.term);
  });

  test('selected folder updates from route', function() {
    navigateTo('/?id=2');
    const action = store.lastAction as SelectFolderAction;
    assertEquals('select-folder', action.name);
    assertEquals('2', action.id);
  });

  test('route updates from ID', async function() {
    store.data.selectedFolder = '2';
    store.notifyObservers();

    await flushTasks();
    assertEquals('chrome://bookmarks/?id=2', window.location.href);
    store.data.selectedFolder = '1';
    store.notifyObservers();
    await flushTasks();
    // Selecting Bookmarks bar clears route.
    assertEquals('chrome://bookmarks/', window.location.href);
  });

  test('route updates from search', async function() {
    store.data.search.term = 'bloop';
    store.notifyObservers();
    await flushTasks();

    assertEquals('chrome://bookmarks/?q=bloop', window.location.href);

    // Ensure that the route doesn't change when the search finishes.
    store.data.selectedFolder = '';
    store.notifyObservers();
    await flushTasks();
    assertEquals('chrome://bookmarks/?q=bloop', window.location.href);
  });

  test('bookmarks bar selected with empty route', function() {
    navigateTo('/?id=2');
    navigateTo('/');
    const action = store.lastAction as SelectFolderAction;
    assertEquals('select-folder', action.name);
    assertEquals('1', action.id);
  });
});

suite('URL preload', function() {
  let testBookmarksApiProxy: TestBookmarksApiProxy;

  setup(function() {
    testBookmarksApiProxy = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(testBookmarksApiProxy);
  });

  /**
   * Reset the page state with a <bookmarks-app> and a clean Store, with the
   * given |url| to trigger routing initialization code.
   */
  function setupWithUrl(url: string) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    Store.setInstance(new Store());
    window.history.replaceState({}, '', url);
    CrRouter.resetForTesting();

    testBookmarksApiProxy.setGetTree([
      createFolder(
          '0',
          [
            createFolder(
                '1',
                [
                  createFolder('11', []),
                ]),
            createFolder(
                '2',
                [
                  createItem('21'),
                ]),
          ]),
    ]);

    const app = document.createElement('bookmarks-app');
    document.body.appendChild(app);
    return flushTasks();
  }

  test('loading a search URL performs a search', async function() {
    testBookmarksApiProxy.setSearchResponse([createItem('11')]);
    await setupWithUrl('/?q=testQuery');
    const lastQuery = await testBookmarksApiProxy.whenCalled('search');
    assertEquals('testQuery', lastQuery);
  });

  test('loading a folder URL selects that folder', async function() {
    await setupWithUrl('/?id=2');
    const state = Store.getInstance().data;
    assertEquals('2', state.selectedFolder);
    assertDeepEquals(['21'], getDisplayedList(state));
  });

  test(
      'loading an invalid folder URL selects the Bookmarks Bar',
      async function() {
        await setupWithUrl('/?id=42');
        const state = Store.getInstance().data;
        assertEquals('1', state.selectedFolder);
        return Promise.resolve().then(function() {
          assertEquals('chrome://bookmarks/', window.location.href);
        });
      });
});
