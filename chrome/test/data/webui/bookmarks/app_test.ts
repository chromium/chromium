// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksAppElement, MojoRootNode} from 'chrome://bookmarks/bookmarks.js';
import {ACCOUNT_HEADING_NODE_ID, BookmarksApiProxyImpl, HIDE_FOCUS_RING_ATTRIBUTE, LOCAL_HEADING_NODE_ID, LOCAL_STORAGE_FOLDER_STATE_KEY, LOCAL_STORAGE_TREE_WIDTH_KEY, normalizeMojoNode, PermanentFolderType, ROOT_NODE_ID} from 'chrome://bookmarks/bookmarks.js';
import {COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn, pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {down} from 'chrome://webui-test/mouse_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, createRoot, normalizeIterable, replaceBody} from './test_util.js';

suite('<bookmarks-app>', function() {
  let app: BookmarksAppElement;
  let store: TestStore;
  let testBookmarksApiProxy: TestBookmarksApiProxy;

  function resetStore() {
    store = new TestStore({});
    store.acceptInitOnce();
    store.replaceSingleton();
    testBookmarksApiProxy.setGetTree(createRoot([
      createFolder(
          '1',
          [
            createFolder('11', []),
          ],
          {
            permanentFolderType: PermanentFolderType.kBookmarkBar,
          }),
    ]));
  }

  setup(function() {
    window.localStorage.clear();
    testBookmarksApiProxy = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(testBookmarksApiProxy);
    resetStore();

    app = document.createElement('bookmarks-app');
    replaceBody(app);
    return microtasksFinished();
  });

  teardown(function() {
    // Teardown the element to ensure it is disconnected from the DOM, which
    // removes event listeners from the active BookmarksApiProxy instance.
    // This prevents the element from trying to remove listeners from a swapped
    // proxy instance in subsequent test setups.
    app.remove();
    replaceBody(document.createElement('div'));
  });

  test('write and load closed folder state', async function() {
    const folderOpenStateList = [['1', true] as const];
    const folderOpenState = new Map(folderOpenStateList);
    store.data.folderOpenState = folderOpenState;
    store.notifyObservers();
    await microtasksFinished();

    // Ensure closed folders are written to local storage.
    assertDeepEquals(
        JSON.stringify(Array.from(folderOpenState)),
        window.localStorage[LOCAL_STORAGE_FOLDER_STATE_KEY]);

    resetStore();
    app = document.createElement('bookmarks-app');
    replaceBody(app);
    await microtasksFinished();

    // Ensure closed folders are read from local storage.
    assertDeepEquals(
        folderOpenStateList, normalizeIterable(store.data.folderOpenState));
  });

  test('write and load sidebar width', async function() {
    assertEquals(
        getComputedStyle(app.$.sidebar).width,
        app.shadowRoot.querySelector('bookmarks-toolbar')!.sidebarWidth);

    const sidebarWidth = '500px';
    app.$.sidebar.style.width = sidebarWidth;
    app.$.splitter.dispatchEvent(new CustomEvent('resize'));
    assertEquals(
        sidebarWidth, window.localStorage[LOCAL_STORAGE_TREE_WIDTH_KEY]);

    app = document.createElement('bookmarks-app');
    replaceBody(app);
    await microtasksFinished();

    assertEquals(sidebarWidth, app.$.sidebar.style.width);
  });

  test('focus ring hides and restores', async function() {
    const list = app.shadowRoot.querySelector('bookmarks-list');
    assertTrue(!!list);
    await microtasksFinished();
    const item = list.shadowRoot.querySelectorAll('bookmarks-item')[0];
    assertTrue(!!item);
    const hasFocusAttribute = () => app.hasAttribute(HIDE_FOCUS_RING_ATTRIBUTE);

    assertFalse(hasFocusAttribute());

    down(item);
    assertTrue(hasFocusAttribute());

    keyDownOn(item, 16, [], 'Shift');
    assertTrue(hasFocusAttribute());

    // This event is also captured by the bookmarks-list and propagation is
    // stopped. Regardless, it should clear the focus first.
    keyDownOn(item, 40, [], 'ArrowDown');
    assertFalse(hasFocusAttribute());
  });

  test('when find shortcut is invoked, focus on search input', async () => {
    const searchField =
        app.shadowRoot.querySelector('bookmarks-toolbar')!.searchField;
    const searchInput = searchField.getSearchInput();
    searchInput.blur();
    assertNotEquals(searchInput, getDeepActiveElement());
    pressAndReleaseKeyOn(document.body, 0, isMac ? 'meta' : 'ctrl', 'f');
    await searchField.updateComplete;
    assertEquals(searchInput, getDeepActiveElement());
  });
});

suite('WebuiRefresh2026', function() {
  const WEBUI_REFRESH_ATTR = 'webui-refresh-2026';
  let app: BookmarksAppElement;
  let store: TestStore;
  let testBookmarksApiProxy: TestBookmarksApiProxy;

  setup(function() {
    window.localStorage.clear();
    testBookmarksApiProxy = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(testBookmarksApiProxy);

    store = new TestStore({});
    store.acceptInitOnce();
    store.replaceSingleton();
    testBookmarksApiProxy.setGetTree(createRoot([
      createFolder(
          '1',
          [
            createFolder('11', []),
          ],
          {
            permanentFolderType: PermanentFolderType.kBookmarkBar,
          }),
    ]));
  });

  teardown(function() {
    replaceBody(document.createElement('div'));
  });

  async function createApp() {
    app = document.createElement('bookmarks-app');
    replaceBody(app);
    return microtasksFinished();
  }

  test('Enabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: WEBUI_REFRESH_ATTR});
    await createApp();

    assertNotEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });

  test('Disabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: ''});
    await createApp();

    assertEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });
});

suite('SignInEdgeCases', function() {
  let app: BookmarksAppElement;
  let store: TestStore;
  let testBookmarksApiProxy: TestBookmarksApiProxy;

  async function setupApp(tree: MojoRootNode) {
    testBookmarksApiProxy = new TestBookmarksApiProxy();
    testBookmarksApiProxy.setGetTree(tree);
    BookmarksApiProxyImpl.setInstance(testBookmarksApiProxy);

    store = new TestStore({});
    store.acceptInitOnce();
    store.setReducersEnabled(true);
    store.replaceSingleton();

    app = document.createElement('bookmarks-app');
    replaceBody(app);
    await testBookmarksApiProxy.whenCalled('getTree');
    await microtasksFinished();
  }

  setup(function() {
    window.localStorage.clear();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('only local nodes (signed out)', async () => {
    await setupApp(createRoot([
      createFolder('1', [createItem('11')], {
        title: 'Bookmarks bar',
        permanentFolderType: PermanentFolderType.kBookmarkBar,
        isSynced: false,
      }),
      createFolder('2', [], {
        title: 'Other bookmarks',
        permanentFolderType: PermanentFolderType.kOther,
        isSynced: false,
      }),
    ]));

    // Verify heading nodes are not synthesized.
    assertFalse(ACCOUNT_HEADING_NODE_ID in store.data.nodes);
    assertFalse(LOCAL_HEADING_NODE_ID in store.data.nodes);

    // Verify default selection is the local bookmarks bar.
    assertEquals('1', store.data.selectedFolder);
    assertDeepEquals(['1', '2'], store.data.nodes[ROOT_NODE_ID]!.children);
  });

  test('local and account nodes simultaneously (signed in)', async () => {
    await setupApp(createRoot([
      createFolder('account_bb', [createItem('acc_item')], {
        title: 'Bookmarks bar',
        permanentFolderType: PermanentFolderType.kBookmarkBar,
        isSynced: true,
      }),
      createFolder('account_other', [], {
        title: 'Other bookmarks',
        permanentFolderType: PermanentFolderType.kOther,
        isSynced: true,
      }),
      createFolder('local_bb', [createItem('loc_item')], {
        title: 'Bookmarks bar',
        permanentFolderType: PermanentFolderType.kBookmarkBar,
        isSynced: false,
      }),
      createFolder('local_other', [], {
        title: 'Other bookmarks',
        permanentFolderType: PermanentFolderType.kOther,
        isSynced: false,
      }),
    ]));

    // Verify heading nodes are synthesized.
    assertTrue(ACCOUNT_HEADING_NODE_ID in store.data.nodes);
    assertTrue(LOCAL_HEADING_NODE_ID in store.data.nodes);

    // Verify account and local folders are categorized under respective
    // headings.
    assertDeepEquals(
        ['account_bb', 'account_other'],
        store.data.nodes[ACCOUNT_HEADING_NODE_ID]!.children);
    assertDeepEquals(
        ['local_bb', 'local_other'],
        store.data.nodes[LOCAL_HEADING_NODE_ID]!.children);

    // Verify account bookmarks bar takes priority for default selection.
    assertEquals('account_bb', store.data.selectedFolder);
  });

  test('login happens afterwards (dynamic account nodes added)', async () => {
    await setupApp(createRoot([
      createFolder('1', [createItem('11')], {
        title: 'Bookmarks bar',
        permanentFolderType: PermanentFolderType.kBookmarkBar,
        isSynced: false,
      }),
      createFolder('2', [], {
        title: 'Other bookmarks',
        permanentFolderType: PermanentFolderType.kOther,
        isSynced: false,
      }),
    ]));

    assertEquals('1', store.data.selectedFolder);

    // Simulate dynamically receiving a new account bookmark folder and item
    // after sign-in.
    const accountFolder = createFolder('acc_folder', [], {isSynced: true});
    testBookmarksApiProxy.onCreated.callListeners(
        '1', 0, normalizeMojoNode(accountFolder, '1'));
    await microtasksFinished();

    assertTrue('acc_folder' in store.data.nodes);
    assertEquals('1', store.data.nodes['acc_folder'].parentId);
    assertTrue(!!store.data.nodes['acc_folder'].isSynced);

    const accountItem =
        createItem('acc_item', {isSynced: true, url: 'https://google.com'});
    testBookmarksApiProxy.onCreated.callListeners(
        'acc_folder', 0, normalizeMojoNode(accountItem, 'acc_folder'));
    await microtasksFinished();

    assertTrue('acc_item' in store.data.nodes);
    assertEquals('acc_folder', store.data.nodes['acc_item'].parentId);
  });

  test(
      'sign-out afterwards (dynamic account node removal and heading pruning)',
      async () => {
        await setupApp(createRoot([
          createFolder('account_bb', [], {
            title: 'Bookmarks bar',
            permanentFolderType: PermanentFolderType.kBookmarkBar,
            isSynced: true,
          }),
          createFolder('local_bb', [], {
            title: 'Bookmarks bar',
            permanentFolderType: PermanentFolderType.kBookmarkBar,
            isSynced: false,
          }),
        ]));

        assertEquals('account_bb', store.data.selectedFolder);
        assertTrue(ACCOUNT_HEADING_NODE_ID in store.data.nodes);

        // Simulate account bookmarks removal on signout.
        testBookmarksApiProxy.onRemoved.callListeners(
            'account_bb', ROOT_NODE_ID, 0);
        await microtasksFinished();

        // Heading nodes should be pruned, reverting to single local tree.
        assertFalse(ACCOUNT_HEADING_NODE_ID in store.data.nodes);
        assertFalse(LOCAL_HEADING_NODE_ID in store.data.nodes);
        assertEquals('local_bb', store.data.selectedFolder);
        assertDeepEquals(
            ['local_bb'], store.data.nodes[ROOT_NODE_ID]!.children);
      });
});
