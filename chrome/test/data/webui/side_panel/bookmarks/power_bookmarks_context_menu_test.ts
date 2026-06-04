// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_context_menu.js';

import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {MenuItemId} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_context_menu.js';
import type {MenuItem, PowerBookmarksContextMenuElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_context_menu.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestPowerBookmarksDelegate} from './test_power_bookmarks_delegate.js';

suite('SidePanelPowerBookmarksContextMenuTest', () => {
  let powerBookmarksContextMenu: PowerBookmarksContextMenuElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let delegate: TestPowerBookmarksDelegate;
  let service: PowerBookmarksService;

  const allBookmarks: BookmarksTreeNode[] = [
    {
      id: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
      parentId: 'SIDE_PANEL_ROOT_BOOKMARK_ID',
      index: 0,
      title: 'Other Bookmarks',
      url: null,
      dateAdded: null,
      dateLastUsed: null,
      unmodifiable: false,
      children: [
        {
          id: '3',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 0,
          title: 'First child bookmark',
          url: 'http://child/bookmark/1/',
          dateAdded: 1,
          dateLastUsed: null,
          unmodifiable: false,
          children: null,
        },
        {
          id: '4',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 1,
          title: 'Second child bookmark',
          url: 'http://child/bookmark/2/',
          dateAdded: 3,
          dateLastUsed: null,
          unmodifiable: false,
          children: null,
        },
        {
          id: '5',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          title: 'Child folder',
          index: 3,
          dateAdded: 2,
          url: null,
          dateLastUsed: null,
          unmodifiable: false,
          children: [
            {
              id: '6',
              parentId: '5',
              index: 0,
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
              dateAdded: 4,
              dateLastUsed: null,
              unmodifiable: false,
              children: null,
            },
          ],
        },
      ],
    },
  ];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(allBookmarks);
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    delegate = new TestPowerBookmarksDelegate();
    service = new PowerBookmarksService(delegate);
    service.startListening();

    loadTimeData.overrideValues({
      menuOpenNewTab: 'Open in new tab',
      menuOpenNewTabWithCount: 'Open all',
      menuOpenNewWindow: 'Open in new window',
      menuOpenNewWindowWithCount: 'Open all in new window',
      menuOpenIncognito: 'Open in Incognito window',
      menuOpenIncognitoWithCount: 'Open all in Incognito window',
      menuOpenNewTabGroup: 'Open in new tab group',
      menuOpenNewTabGroupWithCount: 'Open all in new tab group',
      menuOpenSplitView: 'Open all in split view',
      menuEdit: 'Edit…',
      menuMoveToBookmarksBar: 'Move to Bookmarks Bar folder',
      menuTrackPrice: 'Track price',
      menuUntrackPrice: 'Untrack price',
      menuRename: 'Rename',
      tooltipDelete: 'Delete',
      tooltipMove: 'Move',
      splitViewEnabled: true,
    });

    powerBookmarksContextMenu =
        document.createElement('power-bookmarks-context-menu');
    document.body.appendChild(powerBookmarksContextMenu);

    await flushTasks();
  });

  test('ShowsMenuItemsForSingleSelectUrl', async () => {
    const selection = [service.findBookmarkWithId('3')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false, 1);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems =
        powerBookmarksContextMenu.shadowRoot.querySelectorAll('.dropdown-item');
    assertEquals(menuItems.length, 7);
    assertTrue(
        menuItems[0]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTab')));
    assertTrue(
        menuItems[1]!.textContent.includes(
            loadTimeData.getString('menuOpenNewWindow')));
    assertTrue(
        menuItems[2]!.textContent.includes(
            loadTimeData.getString('menuOpenSplitView')));
    assertTrue(
        menuItems[3]!.textContent.includes(
            loadTimeData.getString('menuOpenIncognito')));
    assertTrue(
        menuItems[4]!.textContent.includes(loadTimeData.getString('menuEdit')));
    assertTrue(
        menuItems[5]!.textContent.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')));
    assertTrue(
        menuItems[6]!.textContent.includes(
            loadTimeData.getString('tooltipDelete')));
  });

  test('ShowsMenuItemsForSingleSelectFolder', async () => {
    const selection = [service.findBookmarkWithId('5')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false, 1);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems =
        powerBookmarksContextMenu.shadowRoot.querySelectorAll('.dropdown-item');
    assertEquals(menuItems.length, 7);
    assertTrue(
        menuItems[0]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTab')));
    assertTrue(
        menuItems[1]!.textContent.includes(
            loadTimeData.getString('menuOpenNewWindow')));

    let incognito_menu_item_index = 2;
    let new_tab_group_menu_item_index = 3;
    if (loadTimeData.getBoolean('menuSimplification')) {
      incognito_menu_item_index = 3;
      new_tab_group_menu_item_index = 2;
    }
    assertTrue(
        menuItems[incognito_menu_item_index]!.textContent.includes(
            loadTimeData.getString('menuOpenIncognito')));
    assertTrue(
        menuItems[new_tab_group_menu_item_index]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTabGroup')));

    assertTrue(
        menuItems[4]!.textContent.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')));
    assertTrue(
        menuItems[5]!.textContent.includes(
            loadTimeData.getString('menuRename')));
    assertTrue(
        menuItems[6]!.textContent.includes(
            loadTimeData.getString('tooltipDelete')));
  });

  test('ShowsSimplifiedMenuItemsForSingleSelectFolder', async () => {
    loadTimeData.overrideValues({
      menuSimplification: true,
      bookmarksBarId: '1',
      otherBookmarksId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
      mobileBookmarksId: '2',
    });

    const selection = [service.findBookmarkWithId('5')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false, 1);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems =
        powerBookmarksContextMenu.shadowRoot.querySelectorAll('.dropdown-item');
    assertEquals(menuItems.length, 7);
    assertTrue(
        menuItems[0]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTab')));
    assertTrue(
        menuItems[1]!.textContent.includes(
            loadTimeData.getString('menuOpenNewWindow')));
    assertTrue(
        menuItems[2]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTabGroup')));
    assertTrue(
        menuItems[3]!.textContent.includes(
            loadTimeData.getString('menuOpenIncognito')));
    assertTrue(
        menuItems[4]!.textContent.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')));
    assertTrue(
        menuItems[5]!.textContent.includes(
            loadTimeData.getString('menuRename')));
    assertTrue(
        menuItems[6]!.textContent.includes(
            loadTimeData.getString('tooltipDelete')));
  });

  test('ShowsSimplifiedMenuItemsForFolderInBookmarksBar', async () => {
    loadTimeData.overrideValues({
      menuSimplification: true,
      bookmarksBarId: '1',
      otherBookmarksId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
      mobileBookmarksId: '2',
    });

    const folderInBar: BookmarksTreeNode = {
      id: '10',
      parentId: '1',
      index: 0,
      title: 'Folder in Bar',
      url: null,
      dateAdded: null,
      dateLastUsed: null,
      unmodifiable: false,
      children: [],
    };

    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), [folderInBar], false, false, false, 0);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems = powerBookmarksContextMenu['getMenuItemsForBookmarks_']();

    const itemIds = menuItems.map((item: MenuItem) => item.id);
    const expectedIds = [
      MenuItemId.OPEN_NEW_TAB,
      MenuItemId.OPEN_NEW_WINDOW,
      MenuItemId.OPEN_NEW_TAB_GROUP,
      MenuItemId.OPEN_INCOGNITO,
      MenuItemId.DIVIDER,
      MenuItemId.RENAME,
      MenuItemId.DIVIDER,
      MenuItemId.DELETE,
    ];

    assertDeepEquals(expectedIds, itemIds);
  });

  test('ShowsMenuItemsForMultiSelect', async () => {
    const selection =
        [service.findBookmarkWithId('3')!, service.findBookmarkWithId('4')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false, 2);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems =
        powerBookmarksContextMenu.shadowRoot.querySelectorAll('.dropdown-item');
    assertEquals(menuItems.length, 6);
    assertTrue(
        menuItems[0]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTabWithCount')));
    assertTrue(
        menuItems[1]!.textContent.includes(
            loadTimeData.getString('menuOpenNewWindowWithCount')));
    assertTrue(
        menuItems[2]!.textContent.includes(
            loadTimeData.getString('menuOpenIncognitoWithCount')));
    assertTrue(
        menuItems[3]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTabGroupWithCount')));
    assertTrue(
        menuItems[4]!.textContent.includes(
            loadTimeData.getString('tooltipMove')));
    assertTrue(
        menuItems[5]!.textContent.includes(
            loadTimeData.getString('tooltipDelete')));
  });

  test('ShowsMenuItemsForPriceTracking', async () => {
    const selection = [service.findBookmarkWithId('4')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, true, true, false, 1);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems =
        powerBookmarksContextMenu.shadowRoot.querySelectorAll('.dropdown-item');
    assertEquals(menuItems.length, 8);
    assertTrue(
        menuItems[0]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTab')));
    assertTrue(
        menuItems[1]!.textContent.includes(
            loadTimeData.getString('menuOpenNewWindow')));
    assertTrue(
        menuItems[2]!.textContent.includes(
            loadTimeData.getString('menuOpenSplitView')));
    assertTrue(
        menuItems[3]!.textContent.includes(
            loadTimeData.getString('menuOpenIncognito')));
    assertTrue(
        menuItems[4]!.textContent.includes(loadTimeData.getString('menuEdit')));
    assertTrue(
        menuItems[5]!.textContent.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')));
    assertTrue(
        menuItems[6]!.textContent.includes(
            loadTimeData.getString('menuUntrackPrice')));
    assertTrue(
        menuItems[7]!.textContent.includes(
            loadTimeData.getString('tooltipDelete')));
  });

  test('ShowsMenuItemsForUserWithIncognitoDisabled', async () => {
    loadTimeData.overrideValues({
      isIncognitoModeAvailable: false,
    });

    const selection = [service.findBookmarkWithId('5')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false, 1);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems =
        powerBookmarksContextMenu.shadowRoot.querySelectorAll('.dropdown-item');
    assertEquals(menuItems.length, 6);
    assertTrue(
        menuItems[0]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTab')));
    assertTrue(
        menuItems[1]!.textContent.includes(
            loadTimeData.getString('menuOpenNewWindow')));
    assertTrue(
        menuItems[2]!.textContent.includes(
            loadTimeData.getString('menuOpenNewTabGroup')));
    assertTrue(
        menuItems[3]!.textContent.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')));
    assertTrue(
        menuItems[4]!.textContent.includes(
            loadTimeData.getString('menuRename')));
    assertTrue(
        menuItems[5]!.textContent.includes(
            loadTimeData.getString('tooltipDelete')));
  });

  test('ShowsMenuItemsWithIncognitoDisabledForNotAllowedUrls', async () => {
    loadTimeData.overrideValues({
      isIncognitoModeAvailable: true,
    });

    const selection = [service.findBookmarkWithId('3')!];
    // Set incognito allowed count to 0.
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false, 0);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems =
        powerBookmarksContextMenu.shadowRoot.querySelectorAll('.dropdown-item');
    assertEquals(menuItems.length, 7);
    const incognitoButton = menuItems[3] as HTMLButtonElement;
    assertTrue(incognitoButton.textContent.includes(
        loadTimeData.getString('menuOpenIncognito')));
    assertTrue(incognitoButton.disabled);
  });

  test('DismissOnBlur', async () => {
    const selection = [service.findBookmarkWithId('3')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false, 1);

    await waitAfterNextRender(powerBookmarksContextMenu);

    assertTrue(powerBookmarksContextMenu.isOpen());

    // Simulate blur by dispatching focusout event with relatedTarget outside
    // the menu.
    const event = new FocusEvent('focusout', {
      relatedTarget: document.body,
    });
    powerBookmarksContextMenu.shadowRoot.querySelector('#menu')!.dispatchEvent(
        event);

    await flushTasks();

    assertTrue(!powerBookmarksContextMenu.isOpen());
  });
});
