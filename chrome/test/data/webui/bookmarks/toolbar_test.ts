// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksToolbarElement} from 'chrome://bookmarks/bookmarks.js';
import {BookmarkManagerApiProxyImpl, Command} from 'chrome://bookmarks/bookmarks.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';

import {TestBookmarkManagerApiProxy} from './test_bookmark_manager_api_proxy.js';
import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-toolbar>', function() {
  let toolbar: BookmarksToolbarElement;
  let store: TestStore;
  let testCommandManager: TestCommandManager;

  suiteSetup(function() {
    const bookmarkManagerApi = new TestBookmarkManagerApiProxy();
    BookmarkManagerApiProxyImpl.setInstance(bookmarkManagerApi);
  });

  setup(function() {
    const nodes = testTree(createFolder('1', [
      createItem('2'),
      createItem('3'),
      createFolder('4', [], {
        unmodifiable: chrome.bookmarks.BookmarkTreeNodeUnmodifiable.MANAGED,
      }),
      createFolder('5', []),
      createFolder(
          '6',
          [
            createItem('61'),
            createItem('62'),
          ]),
    ]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selection: {
        items: new Set(),
        anchor: null,
      },
    });
    store.replaceSingleton();

    toolbar = document.createElement('bookmarks-toolbar');
    replaceBody(toolbar);

    testCommandManager = new TestCommandManager();
    document.body.appendChild(testCommandManager.getCommandManager());

    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('selecting multiple items shows toolbar overlay', function() {
    assertFalse(toolbar.showSelectionOverlay);

    store.data.selection.items = new Set(['2']);
    store.notifyObservers();
    assertFalse(toolbar.showSelectionOverlay);

    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();
    assertTrue(toolbar.showSelectionOverlay);
  });

  test('overlay does not show when editing is disabled', function() {
    store.data.prefs.canEdit = false;
    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();
    assertFalse(toolbar.showSelectionOverlay);
  });

  test('clicking overlay delete button triggers a delete command', function() {
    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();

    flush();
    const button =
        toolbar.shadowRoot!.querySelector('cr-toolbar-selection-overlay')!
            .querySelector('cr-button')!;
    assertFalse(button.disabled);
    button.click();

    testCommandManager.assertLastCommand(Command.DELETE, ['2', '3']);
  });

  test('commands do not trigger from the search field', function() {
    store.data.selection.items = new Set(['2']);
    store.notifyObservers();

    const input =
        toolbar.shadowRoot!.querySelector('cr-toolbar')!.getSearchField()
            .getSearchInput();
    const modifier = isMac ? 'meta' : 'ctrl';
    pressAndReleaseKeyOn(input, 67, modifier, 'c');

    testCommandManager.assertLastCommand(null);
  });

  test('delete button is disabled when items are unmodifiable', function() {
    store.data.nodes['3']!.unmodifiable = 'managed';
    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();
    flush();

    assertTrue(toolbar.showSelectionOverlay);
    assertTrue(
        toolbar.shadowRoot!.querySelector('cr-toolbar-selection-overlay')!
            .querySelector('cr-button')!.disabled);
  });
});
