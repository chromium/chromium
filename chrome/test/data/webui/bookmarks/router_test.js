// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDisplayedList, Store} from 'chrome://bookmarks/bookmarks.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-router>', function() {
  let store;
  let router;

  function navigateTo(route) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('location-changed'));
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

    router = document.createElement('bookmarks-router');
    replaceBody(router);
  });

  test('search updates from route', function() {
    navigateTo('/?q=bleep');
    assertEquals('start-search', store.lastAction.name);
    assertEquals('bleep', store.lastAction.term);
  });

  test('selected folder updates from route', function() {
    navigateTo('/?id=2');
    assertEquals('select-folder', store.lastAction.name);
    assertEquals('2', store.lastAction.id);
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
    store.data.search = {term: 'bloop'};
    store.notifyObservers();
    await flushTasks();

    assertEquals('chrome://bookmarks/?q=bloop', window.location.href);

    // Ensure that the route doesn't change when the search finishes.
    store.data.selectedFolder = null;
    store.notifyObservers();
    await flushTasks();
    assertEquals('chrome://bookmarks/?q=bloop', window.location.href);
  });

  test('bookmarks bar selected with empty route', function() {
    navigateTo('/?id=2');
    navigateTo('/');
    assertEquals('select-folder', store.lastAction.name);
    assertEquals('1', store.lastAction.id);
  });
});

suite('URL preload', function() {
  /**
   * Reset the page state with a <bookmarks-app> and a clean Store, with the
   * given |url| to trigger routing initialization code.
   */
  function setupWithUrl(url) {
    document.body.innerHTML = '';
    Store.setInstance(undefined);
    window.history.replaceState({}, '', url);

    chrome.bookmarks.getTree = function(callback) {
      callback([
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
    };

    const app = document.createElement('bookmarks-app');
    document.body.appendChild(app);
  }

  test('loading a search URL performs a search', function() {
    let lastQuery;
    chrome.bookmarks.search = function(query) {
      lastQuery = query;
      return ['11'];
    };

    setupWithUrl('/?q=testQuery');
    assertEquals('testQuery', lastQuery);
  });

  test('loading a folder URL selects that folder', function() {
    setupWithUrl('/?id=2');
    const state = Store.getInstance().data;
    assertEquals('2', state.selectedFolder);
    assertDeepEquals(['21'], getDisplayedList(state));
  });

  test('loading an invalid folder URL selects the Bookmarks Bar', function() {
    setupWithUrl('/?id=42');
    const state = Store.getInstance().data;
    assertEquals('1', state.selectedFolder);
    return Promise.resolve().then(function() {
      assertEquals('chrome://bookmarks/', window.location.href);
    });
  });
});
