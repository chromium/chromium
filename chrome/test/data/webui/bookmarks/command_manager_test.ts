// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksFolderNodeElement, BookmarksItemElement, BookmarksListElement, SelectFolderAction, SelectItemsAction} from 'chrome://bookmarks/bookmarks.js';
import {BookmarkManagerApiProxyImpl, BookmarksApiProxyImpl, BookmarksCommandManagerElement, Command, createBookmark, DialogFocusManager, getDisplayedList, MenuSource, selectFolder, setDebouncerForTesting} from 'chrome://bookmarks/bookmarks.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {ModifiersParam} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarkManagerApiProxy} from './test_bookmark_manager_api_proxy.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, customClick, findFolderNode, normalizeIterable, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-command-manager>', function() {
  let commandManager: BookmarksCommandManagerElement;
  let testCommandManager: TestCommandManager;
  let store: TestStore;
  let bookmarkManagerProxy: TestBookmarkManagerApiProxy;

  setup(function() {
    const bulkChildren = [];
    for (let i = 1; i <= 20; i++) {
      const id = '3' + i;
      bulkChildren.push(createItem(id, {url: `http://${id}/`}));
    }

    store = new TestStore({
      nodes: testTree(
          createFolder(
              '1',
              [
                createFolder(
                    '11',
                    [
                      createItem('111', {url: 'http://111/'}),
                    ]),
                createFolder(
                    '12',
                    [
                      createItem('121', {url: 'http://121/'}),
                      createFolder(
                          '122',
                          [
                            createItem('1221'),
                          ]),
                    ]),
                createItem('13', {url: 'http://13/'}),
                createFolder(
                    '14',
                    [
                      createItem('141'),
                      createItem('142'),
                    ]),
              ]),
          createFolder(
              '2',
              [
                createFolder('21', []),
              ]),
          createFolder('3', bulkChildren), createFolder('4', [], {
            unmodifiable: chrome.bookmarks.BookmarkTreeNodeUnmodifiable.MANAGED,
          })),
      selectedFolder: '1',
    });
    store.replaceSingleton();

    const bookmarksProxy = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(bookmarksProxy);

    bookmarkManagerProxy = new TestBookmarkManagerApiProxy();
    BookmarkManagerApiProxyImpl.setInstance(bookmarkManagerProxy);

    testCommandManager = new TestCommandManager();
    commandManager = testCommandManager.getCommandManager();
    replaceBody(commandManager);
    document.body.appendChild(document.createElement('cr-toast-manager'));
    DialogFocusManager.setInstance(null);
  });

  test('context menu hides invalid commands', function() {
    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    flush();

    const commandHidden: {[key: string]: boolean} = {};
    commandManager.shadowRoot!.querySelectorAll<HTMLElement>('.dropdown-item')
        .forEach(element => {
          commandHidden[element.getAttribute('command')!] = element.hidden;
        });

    // With a folder and an item selected, the only available context menu item
    // is 'Delete'.
    assertTrue(commandHidden[Command.EDIT] !== undefined);
    assertTrue(commandHidden[Command.EDIT]);

    assertTrue(commandHidden[Command.DELETE] !== undefined);
    assertFalse(commandHidden[Command.DELETE]);
  });

  test('edit shortcut triggers when valid', function() {
    const key = isMac ? 'Enter' : 'F2';

    store.data.selection.items = new Set(['13']);
    store.notifyObservers();

    pressAndReleaseKeyOn(document.body, 0, [], key);
    testCommandManager.assertLastCommand(Command.EDIT, ['13']);

    // Doesn't trigger when multiple items are selected.
    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    pressAndReleaseKeyOn(document.body, 0, [], key);
    testCommandManager.assertLastCommand(null);

    // Doesn't trigger when nothing is selected.
    store.data.selection.items = new Set();
    store.notifyObservers();

    pressAndReleaseKeyOn(document.body, 0, [], key);
    testCommandManager.assertLastCommand(null);
  });

  test('delete command triggers', function() {
    store.data.selection.items = new Set(['12', '13']);
    store.notifyObservers();

    pressAndReleaseKeyOn(document.body, 46, [], 'Delete');
    testCommandManager.assertLastCommand(Command.DELETE, ['12', '13']);
  });

  test('copy command triggers', function() {
    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    document.dispatchEvent(new Event('copy'));
    testCommandManager.assertLastCommand(Command.COPY, ['11', '13']);
  });

  test('cut/paste commands trigger', async function() {
    store.data.selection.items = new Set(['11', '13']);
    store.notifyObservers();

    document.dispatchEvent(new Event('cut'));
    const lastCut = (await bookmarkManagerProxy.whenCalled('cut')).sort();
    assertDeepEquals(['11', '13'], lastCut);

    setDebouncerForTesting();
    document.dispatchEvent(new Event('paste'));
    const lastPaste = await bookmarkManagerProxy.whenCalled('paste');
    assertEquals('1', lastPaste);
  });

  test('undo and redo commands trigger', function() {
    const undoModifier = isMac ? 'meta' : 'ctrl';
    const undoKey = 'z';
    const redoModifier: ModifiersParam = isMac ? ['meta', 'shift'] : 'ctrl';
    const redoKey = isMac ? 'Z' : 'y';

    pressAndReleaseKeyOn(document.body, 0, undoModifier, undoKey);
    testCommandManager.assertLastCommand(Command.UNDO);

    pressAndReleaseKeyOn(document.body, 0, redoModifier, redoKey);
    testCommandManager.assertLastCommand(Command.REDO);
  });

  test('undo triggered when bookmarks-toolbar element has focus', function() {
    const element = document.createElement('bookmarks-toolbar');
    document.body.appendChild(element);
    pressAndReleaseKeyOn(element, 0, isMac ? 'meta' : 'ctrl', 'z');
    testCommandManager.assertLastCommand(Command.UNDO);
  });

  test('undo not triggered when most other elements have focus', function() {
    const element = document.createElement('div');
    document.body.appendChild(element);
    pressAndReleaseKeyOn(element, 0, isMac ? 'meta' : 'ctrl', 'z');
    testCommandManager.assertLastCommand(null);
  });

  test('undo not triggered when toolbar input has focus', function() {
    const toolbar = document.createElement('bookmarks-toolbar');
    const input = document.createElement('input');
    toolbar.appendChild(input);
    document.body.appendChild(toolbar);
    pressAndReleaseKeyOn(input, 0, isMac ? 'meta' : 'ctrl', 'z');
    testCommandManager.assertLastCommand(null);
  });

  test('Show In Folder is only available during search', function() {
    store.data.selection.items = new Set(['12']);
    store.notifyObservers();

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    flush();

    const showInFolderItem =
        commandManager.shadowRoot!.querySelector<HTMLElement>(
            `[command='${Command.SHOW_IN_FOLDER}']`);

    // Show in folder hidden when search is inactive.
    assertTrue(!!showInFolderItem);
    assertTrue(showInFolderItem.hidden);

    // Show in Folder visible when search is active.
    store.data.search.term = 'test';
    store.data.search.results = ['12', '13'];
    store.notifyObservers();
    commandManager.closeCommandMenu();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    assertFalse(showInFolderItem.hidden);

    // Show in Folder hidden when menu is opened from the sidebar.
    commandManager.closeCommandMenu();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.TREE);
    assertTrue(showInFolderItem.hidden);

    // Show in Folder hidden when multiple items are selected.
    store.data.selection.items = new Set(['12', '13']);
    store.notifyObservers();
    commandManager.closeCommandMenu();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    assertTrue(showInFolderItem.hidden);

    // Executing the command selects the parent folder.
    commandManager.handle(Command.SHOW_IN_FOLDER, new Set(['12']));
    assertTrue(!!store.lastAction);
    assertEquals('select-folder', store.lastAction.name);
    assertEquals('1', (store.lastAction as SelectFolderAction).id);
  });

  test('does not delete children at same time as ancestor', async function() {
    const parentAndChildren = new Set(['11', '12', '111', '1221']);
    assertTrue(commandManager.canExecute(Command.DELETE, parentAndChildren));
    commandManager.handle(Command.DELETE, parentAndChildren);

    const lastDelete = await bookmarkManagerProxy.whenCalled('removeTrees');

    assertDeepEquals(['11', '12'], lastDelete);
  });

  test('shift-enter opens URLs in new window', async function() {
    store.data.selection.items = new Set(['12', '13']);
    store.notifyObservers();

    pressAndReleaseKeyOn(document.body, 13, 'shift', 'Enter');
    const [ids, incognito] =
        await bookmarkManagerProxy.whenCalled('openInNewWindow');
    testCommandManager.assertLastCommand(Command.OPEN_NEW_WINDOW, ['12', '13']);
    assertDeepEquals(['121', '13'], ids);
    assertFalse(incognito);
  });

  test('shift-enter does not trigger enter commands', function() {
    // Enter by itself performs an edit (Mac) or open (non-Mac). Ensure that
    // shift-enter doesn't trigger those commands.
    store.data.selection.items = new Set(['13']);
    store.notifyObservers();

    pressAndReleaseKeyOn(document.body, 13, 'shift', 'Enter');
    testCommandManager.assertLastCommand(Command.OPEN_NEW_WINDOW);
  });

  test('opening many items causes a confirmation dialog', async function() {
    const items = new Set(['3']);
    assertTrue(commandManager.canExecute(Command.OPEN_NEW_WINDOW, items));

    commandManager.handle(Command.OPEN_NEW_WINDOW, items);

    const dialog = commandManager.$.openDialog.getIfExists();
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    // Pressing 'cancel' should not open the window.
    dialog.querySelector<HTMLElement>('.cancel-button')!.click();
    assertFalse(dialog.open);

    commandManager.handle(Command.OPEN_NEW_WINDOW, items);
    assertTrue(dialog.open);

    // Pressing 'yes' will open all the URLs.
    dialog.querySelector<HTMLElement>('.action-button')!.click();
    const [ids] = await bookmarkManagerProxy.whenCalled('openInNewWindow');
    assertFalse(dialog.open);
    assertEquals(20, ids.length);
  });

  test('cannot execute "Open in New Tab" on folders with no items', function() {
    const items = new Set(['2']);
    assertFalse(commandManager.canExecute(Command.OPEN_NEW_TAB, items));

    store.data.selection.items = items;

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    flush();

    const commandItem: {[key: string]: HTMLButtonElement} = {};
    commandManager.shadowRoot!
        .querySelectorAll<HTMLButtonElement>('.dropdown-item')
        .forEach(element => {
          commandItem[element.getAttribute('command')!] = element;
        });

    assertTrue(!!commandItem[Command.OPEN_NEW_TAB]);
    assertTrue(commandItem[Command.OPEN_NEW_TAB].disabled);
    assertFalse(commandItem[Command.OPEN_NEW_TAB].hidden);

    assertTrue(!!commandItem[Command.OPEN_NEW_WINDOW]);
    assertTrue(commandItem[Command.OPEN_NEW_WINDOW].disabled);
    assertFalse(commandItem[Command.OPEN_NEW_WINDOW].hidden);

    assertTrue(!!commandItem[Command.OPEN_INCOGNITO]);
    assertTrue(commandItem[Command.OPEN_INCOGNITO].disabled);
    assertFalse(commandItem[Command.OPEN_INCOGNITO].hidden);
  });

  test('cannot execute editing commands when editing is disabled', function() {
    const items = new Set(['12']);

    store.data.prefs.canEdit = false;
    store.data.selection.items = items;
    store.notifyObservers();

    assertFalse(commandManager.canExecute(Command.EDIT, items));
    assertFalse(commandManager.canExecute(Command.DELETE, items));
    assertFalse(commandManager.canExecute(Command.UNDO, items));
    assertFalse(commandManager.canExecute(Command.REDO, items));

    // No divider line should be visible when only 'Open' commands are enabled.
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    commandManager.shadowRoot!.querySelectorAll('hr').forEach(element => {
      assertTrue(element.hidden);
    });
  });

  test('cannot edit unmodifiable nodes', function() {
    // Cannot edit root folders.
    let items = new Set(['1']);
    store.data.selection.items = items;
    assertFalse(commandManager.canExecute(Command.EDIT, items));
    assertFalse(commandManager.canExecute(Command.DELETE, items));

    items = new Set(['4']);
    assertFalse(commandManager.canExecute(Command.EDIT, items));
    assertFalse(commandManager.canExecute(Command.DELETE, items));

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    const commandItem: {[key: string]: HTMLElement} = {};
    commandManager.shadowRoot!.querySelectorAll<HTMLElement>('.dropdown-item')
        .forEach(element => {
          commandItem[element.getAttribute('command')!] = element;
        });
    assertTrue(!!commandItem[Command.EDIT]);
    commandItem[Command.EDIT].click();
    testCommandManager.assertLastCommand(null);
  });

  test('keyboard shortcuts are disabled while a dialog is open', function() {
    assertFalse(DialogFocusManager.getInstance().hasOpenDialog());
    const items = new Set(['12']);
    store.data.selection.items = items;
    store.notifyObservers();

    const editKey = isMac ? 'Enter' : 'F2';
    pressAndReleaseKeyOn(document.body, 0, [], editKey);
    testCommandManager.assertLastCommand(Command.EDIT);
    assertTrue(DialogFocusManager.getInstance().hasOpenDialog());

    pressAndReleaseKeyOn(document.body, 0, [], 'Delete');
    testCommandManager.assertLastCommand(null);
  });

  test('toolbar menu options are disabled when appropriate', function() {
    store.data.selectedFolder = '1';
    store.data.prefs.canEdit = true;
    store.notifyObservers();

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.TOOLBAR);
    assertTrue(commandManager.canExecute(Command.SORT, new Set()));
    assertTrue(commandManager.canExecute(Command.ADD_BOOKMARK, new Set()));
    assertTrue(commandManager.canExecute(Command.ADD_FOLDER, new Set()));

    store.data.selectedFolder = '4';
    store.notifyObservers();

    assertFalse(commandManager.canExecute(Command.SORT, new Set()));
    assertFalse(commandManager.canExecute(Command.ADD_BOOKMARK, new Set()));
    assertFalse(commandManager.canExecute(Command.ADD_FOLDER, new Set()));
    assertTrue(commandManager.canExecute(Command.IMPORT, new Set()));

    store.data.selectedFolder = '1';
    store.data.prefs.canEdit = false;
    store.notifyObservers();

    assertFalse(commandManager.canExecute(Command.SORT, new Set()));
    assertFalse(commandManager.canExecute(Command.IMPORT, new Set()));
    assertFalse(commandManager.canExecute(Command.ADD_BOOKMARK, new Set()));
    assertFalse(commandManager.canExecute(Command.ADD_FOLDER, new Set()));
  });

  test('sort button is disabled when folder is empty', function() {
    store.data.selectedFolder = '3';
    store.notifyObservers();

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.TOOLBAR);
    assertTrue(commandManager.canExecute(Command.SORT, new Set()));

    store.data.selectedFolder = '21';
    store.notifyObservers();

    assertFalse(commandManager.canExecute(Command.SORT, new Set()));

    // Adding 2 bookmarks should enable sorting.
    store.setReducersEnabled(true);
    const item1 = {
      id: '211',
      parentId: '21',
      index: 0,
      url: 'https://www.example.com',
      title: 'example',
    };
    store.dispatch(createBookmark(item1.id, item1));
    assertFalse(commandManager.canExecute(Command.SORT, new Set()));

    const item2 = {
      id: '212',
      parentId: '21',
      index: 1,
      url: 'https://www.example.com',
      title: 'example',
    };
    store.dispatch(createBookmark(item2.id, item2));
    assertTrue(commandManager.canExecute(Command.SORT, new Set()));
  });
});

suite('<bookmarks-item> CommandManager integration', function() {
  let list: BookmarksListElement;
  let items: NodeListOf<BookmarksItemElement>;
  let commandManager: BookmarksCommandManagerElement;
  let rootNode: BookmarksFolderNodeElement;
  let store: TestStore;
  let bookmarkManagerProxy: TestBookmarkManagerApiProxy;

  setup(function() {
    store = new TestStore({
      nodes: testTree(createFolder(
          '1',
          [
            createFolder(
                '11',
                [
                  createItem('111', {url: 'http://111/'}),
                ]),
            createItem('12', {url: 'http://12/'}),
            createItem('13', {url: 'http://13/'}),
          ])),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.replaceSingleton();
    bookmarkManagerProxy = new TestBookmarkManagerApiProxy();
    BookmarkManagerApiProxyImpl.setInstance(bookmarkManagerProxy);

    commandManager = document.createElement('bookmarks-command-manager');

    list = document.createElement('bookmarks-list');
    replaceBody(list);
    document.body.appendChild(commandManager);

    rootNode = document.createElement('bookmarks-folder-node');
    rootNode.itemId = '1';
    rootNode.depth = 0;
    document.body.appendChild(rootNode);
    flush();
    document.body.appendChild(document.createElement('cr-toast-manager'));

    items = list.shadowRoot!.querySelectorAll<BookmarksItemElement>(
        'bookmarks-item');
  });

  function simulateDoubleClick(element: HTMLElement, config?: MouseEventInit) {
    config = config || {};
    customClick(element, config);
    config.detail = 2;
    customClick(element, config);
  }

  function simulateMiddleClick(element: HTMLElement, config?: MouseEventInit) {
    config = config || {};
    config.button = 1;
    customClick(element, config, 'auxclick');
  }

  test('double click opens folders in bookmark manager', function() {
    simulateDoubleClick(items[0]!);
    assertEquals(store.data.selectedFolder, '11');
  });

  test('double click opens items in foreground tab', async function() {
    simulateDoubleClick(items[1]!);

    const [id, active] = await bookmarkManagerProxy.whenCalled('openInNewTab');

    assertEquals('12', id);
    assertTrue(active);
  });

  test('shift-double click opens full selection', function() {
    // Shift-double click works because the first click event selects the range
    // of items, then the second doubleclick event opens that whole selection.
    const item1 = items[0];
    const item2 = items[1];

    assertTrue(!!item1);
    assertTrue(!!item2);

    customClick(item1);
    simulateDoubleClick(item2, {shiftKey: true});

    const [id1] = bookmarkManagerProxy.getArgs('openInNewTab')[0];
    const [id2] = bookmarkManagerProxy.getArgs('openInNewTab')[1];

    assertDeepEquals(['11', '12'], [id1, id2]);
  });

  test('control-double click opens full selection', function() {
    const item1 = items[0];
    const item2 = items[2];

    assertTrue(!!item1);
    assertTrue(!!item2);

    customClick(item1);
    simulateDoubleClick(item2, {ctrlKey: true});

    const [id1] = bookmarkManagerProxy.getArgs('openInNewTab')[0];
    const [id2] = bookmarkManagerProxy.getArgs('openInNewTab')[1];

    assertDeepEquals(['11', '13'], [id1, id2]);
  });

  test('middle-click opens clicked item in new tab', async function() {
    const item1 = items[1];
    const item2 = items[2];

    assertTrue(!!item1);
    assertTrue(!!item2);

    // Select multiple items.
    customClick(item1);
    customClick(item2, {shiftKey: true});

    // Only the middle-clicked item is opened.
    simulateMiddleClick(item2);

    const [id, active] = await bookmarkManagerProxy.whenCalled('openInNewTab');

    assertEquals('13', id);
    assertFalse(active);
  });

  test('middle-click does not open folders', function() {
    const item = items[0];
    assertTrue(!!item);

    simulateMiddleClick(item);

    assertDeepEquals(['11'], normalizeIterable(store.data.selection.items));
    assertEquals(0, bookmarkManagerProxy.getCallCount('openInNewTab'));
  });

  test('shift-middle click opens in foreground tab', async function() {
    const item = items[1];
    assertTrue(!!item);

    simulateMiddleClick(item, {shiftKey: true});
    const [id, active] = await bookmarkManagerProxy.whenCalled('openInNewTab');

    assertEquals('12', id);
    assertTrue(active);
  });

  test(
      'copy/cut/paste for folder nodes independent of selection',
      async function() {
        const modifier = isMac ? 'meta' : 'ctrl';

        store.data.selection.items = new Set(['12', '13']);
        store.notifyObservers();
        const targetNode = findFolderNode(rootNode, '11');
        assertTrue(!!targetNode);

        pressAndReleaseKeyOn(targetNode, 0, modifier, 'c');
        const lastCopy = await bookmarkManagerProxy.whenCalled('copy');
        assertDeepEquals(['11'], lastCopy);

        pressAndReleaseKeyOn(targetNode, 0, modifier, 'x');
        const lastCut = await bookmarkManagerProxy.whenCalled('cut');
        assertDeepEquals(['11'], lastCut);
      });

  test('context menu disappears immediately on right click', async function() {
    bookmarkManagerProxy.setCanPaste(true);

    customClick(items[0]!, {button: 1}, 'contextmenu');
    assertDeepEquals(['11'], normalizeIterable(store.data.selection.items));

    await flushTasks();

    const dropdown = commandManager.$.dropdown.getIfExists();
    assertTrue(!!dropdown);

    const dialog = dropdown.getDialog();
    assertTrue(dropdown.open);

    const x = dialog.offsetLeft + dialog.offsetWidth + 5;
    const y = dialog.offsetHeight;

    // Ensure the dialog is the target even when clicking outside it, and send
    // a context menu event which should immediately dismiss the dialog,
    // allowing subsequent events to bubble through to elements below.
    assertEquals(dropdown, commandManager.shadowRoot!.elementFromPoint(x, y));
    assertEquals(dialog, dropdown.shadowRoot!.elementFromPoint(x, y));
    customClick(dialog, {clientX: x, clientY: y, button: 1}, 'contextmenu');
    assertFalse(dropdown.open);
  });
});

suite('<bookmarks-command-manager> whole page integration', function() {
  let store: TestStore;
  let commandManager: BookmarksCommandManagerElement;
  let testFolderId: string;

  function create(details: chrome.bookmarks.CreateDetails) {
    return chrome.bookmarks.create(details);
  }

  suiteSetup(async function() {
    const testFolder = {
      parentId: '1',
      title: 'Test',
    };
    const testFolderNode = await create(testFolder);
    testFolderId = testFolderNode.id;
    const testItem = {
      parentId: testFolderId,
      title: 'Test bookmark',
      url: 'https://www.example.com/',
    };

    await create(testItem);
    await create(testItem);
  });

  setup(async function() {
    const bookmarksProxy = new BookmarksApiProxyImpl();
    BookmarksApiProxyImpl.setInstance(bookmarksProxy);
    const bookmarkManagerProxy = new BookmarkManagerApiProxyImpl();
    BookmarkManagerApiProxyImpl.setInstance(bookmarkManagerProxy);
    store = new TestStore({});
    store.replaceSingleton();
    store.setReducersEnabled(true);
    const promise = store.acceptInitOnce();
    const app = document.createElement('bookmarks-app');
    replaceBody(app);

    commandManager = BookmarksCommandManagerElement.getInstance();

    await promise;

    store.dispatch(selectFolder(testFolderId));
  });

  test('paste selects newly created items', async function() {
    const displayedIdsBefore = getDisplayedList(store.data);

    commandManager.handle(Command.SELECT_ALL, new Set());
    commandManager.handle(Command.COPY, new Set(displayedIdsBefore));

    store.expectAction('select-items');
    commandManager.handle(Command.PASTE, new Set());
    const action =
        await store.waitForAction('select-items') as SelectItemsAction;

    const displayedIdsAfter = getDisplayedList(store.data);
    assertEquals(4, displayedIdsAfter.length);

    // The start of the list shouldn't change.
    assertEquals(displayedIdsBefore[0], displayedIdsAfter[0]);
    assertEquals(displayedIdsBefore[1], displayedIdsAfter[1]);

    // The two pasted items should be selected at the end of the list.
    assertEquals(action.items[0], displayedIdsAfter[2]);
    assertEquals(action.items[1], displayedIdsAfter[3]);
    assertEquals(2, action.items.length);
    assertEquals(action.anchor, displayedIdsAfter[2]);
  });

  suiteTeardown(function() {
    return chrome.bookmarks.removeTree(testFolderId);
  });
});
