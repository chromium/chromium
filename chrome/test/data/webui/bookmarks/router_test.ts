// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksAppElement, SelectFolderAction, StartSearchAction} from 'chrome://bookmarks/bookmarks.js';
import {BookmarksApiProxyImpl, BookmarksRouter, CrRouter, getDisplayedList, PermanentFolderType, Store} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, createRoot, getAllFoldersOpenState, testTree} from './test_util.js';

suite('<bookmarks-router>', function() {
  let store: TestStore;
  let router: BookmarksRouter;

  function navigateTo(route: string) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('popstate'));
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const nodes = testTree(createFolder('1', [createFolder('2', [])]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
      search: {
        term: '',
        inProgress: false,
        results: [],
      },
    });
    store.replaceSingleton();

    router = new BookmarksRouter();
    router.initialize();
  });

  teardown(function() {
    router.teardown();
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
    await microtasksFinished();
    assertEquals('chrome://bookmarks/?id=2', window.location.href);

    store.data.selectedFolder = '1';
    store.notifyObservers();
    await microtasksFinished();
    // Selecting Bookmarks bar clears route.
    assertEquals('chrome://bookmarks/', window.location.href);
  });

  test('route updates from search', async function() {
    store.data.search.term = 'bloop';
    store.notifyObservers();
    await microtasksFinished();

    assertEquals('chrome://bookmarks/?q=bloop', window.location.href);

    // Ensure that the route doesn't change when the search finishes.
    store.data.selectedFolder = '';
    store.notifyObservers();
    await microtasksFinished();
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

suite('<bookmarks-router-account-and-local>', function() {
  let store: TestStore;
  let router: BookmarksRouter;

  function navigateTo(route: string) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('popstate'));
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const nodes = testTree(
        createFolder('1', [createItem('11', {syncing: true})], {
          syncing: true,
          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
        }),
        createFolder('2', [createItem('21', {syncing: false})], {
          syncing: false,
          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
        }));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: 'account_heading',
      search: {
        term: '',
        inProgress: false,
        results: [],
      },
    });
    store.replaceSingleton();

    router = new BookmarksRouter();
    router.initialize();
  });

  teardown(function() {
    router.teardown();
  });

  test('selected folder updates from route', function() {
    navigateTo('/?id=local_heading');
    const action = store.lastAction as SelectFolderAction;
    assertEquals('select-folder', action.name);
    assertEquals('local_heading', action.id);
  });

  test('route updates from ID', async function() {
    store.data.selectedFolder = '2';
    store.notifyObservers();
    await microtasksFinished();
    assertEquals('chrome://bookmarks/?id=2', window.location.href);

    store.data.selectedFolder = 'account_heading';
    store.notifyObservers();
    await microtasksFinished();
    // Selecting account bookmarks root clears route.
    assertEquals('chrome://bookmarks/', window.location.href);
  });

  test('account bookmarks root selected with empty route', function() {
    navigateTo('/?id=2');
    navigateTo('/');
    const action = store.lastAction as SelectFolderAction;
    assertEquals('select-folder', action.name);
    assertEquals('account_heading', action.id);
  });
});

suite('URL preload', function() {
  let testBookmarksApiProxy: TestBookmarksApiProxy;
  let app: BookmarksAppElement;

  setup(function() {
    testBookmarksApiProxy = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(testBookmarksApiProxy);
  });

  teardown(function() {
    // Teardown the element to ensure it is disconnected from the DOM, which
    // removes event listeners from the active BookmarksApiProxy instance.
    // This prevents the element from trying to remove listeners from a swapped
    // proxy instance in subsequent test setups.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

    testBookmarksApiProxy.setGetTree(createRoot([
      createFolder(
          '1',
          [
            createFolder('11', []),
          ],
          {
            permanentFolderType: PermanentFolderType.kBookmarkBar,
          }),
      createFolder(
          '2',
          [
            createItem('21', {title: 'testQuery'}),
          ]),
    ]));

    app = document.createElement('bookmarks-app');
    document.body.appendChild(app);
    return microtasksFinished();
  }

  test('loading a search URL performs a search', async function() {
    await setupWithUrl('/?q=testQuery');
    const state = Store.getInstance().data;
    assertEquals('testQuery', state.search.term);
    assertDeepEquals(['21'], state.search.results);
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
        await microtasksFinished();
        assertEquals('chrome://bookmarks/', window.location.href);
      });
});
