// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksAppElement} from 'chrome://bookmarks/bookmarks.js';
import {BookmarksCommandManagerElement, BrowserProxyImpl, Command, IncognitoAvailability} from 'chrome://bookmarks/bookmarks.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestBookmarksBrowserProxy} from './test_browser_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('Bookmarks policies', function() {
  let store: TestStore;
  let app: BookmarksAppElement;
  let testBrowserProxy: TestBookmarksBrowserProxy;

  setup(function() {
    const nodes = testTree(createFolder('1', [
      createItem('11'),
    ]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.expectAction('set-incognito-availability');
    store.expectAction('set-can-edit');
    store.replaceSingleton();

    testBrowserProxy = new TestBookmarksBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);
    app = document.createElement('bookmarks-app');
    replaceBody(app);
  });

  test('incognito availability updates when changed', async function() {
    const commandManager = BookmarksCommandManagerElement.getInstance();
    // Incognito is disabled during testGenPreamble(). Wait for the front-end to
    // load the config.
    await Promise.all([
      testBrowserProxy.whenCalled('getIncognitoAvailability'),
      store.waitForAction('set-incognito-availability'),
    ]);

    assertEquals(
        IncognitoAvailability.DISABLED, store.data.prefs.incognitoAvailability);
    assertFalse(
        commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));

    webUIListenerCallback(
        'incognito-availability-changed', IncognitoAvailability.ENABLED);
    assertEquals(
        IncognitoAvailability.ENABLED, store.data.prefs.incognitoAvailability);
    assertTrue(
        commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));
  });

  test('canEdit updates when changed', async function() {
    const commandManager = BookmarksCommandManagerElement.getInstance();
    await Promise.all([
      testBrowserProxy.whenCalled('getCanEditBookmarks'),
      store.waitForAction('set-can-edit'),
    ]);
    assertFalse(store.data.prefs.canEdit);
    assertFalse(commandManager.canExecute(Command.DELETE, new Set(['11'])));

    webUIListenerCallback('can-edit-bookmarks-changed', true);
    assertTrue(store.data.prefs.canEdit);
    assertTrue(commandManager.canExecute(Command.DELETE, new Set(['11'])));
  });
});
