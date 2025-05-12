// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_context_menu.js';

import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {PowerBookmarksContextMenuElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_context_menu.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
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
        new MouseEvent('click'), selection, false, false, false);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems = powerBookmarksContextMenu.shadowRoot!.querySelectorAll(
        '.dropdown-item');
    assertEquals(menuItems.length, 7);
    assertEquals(
        menuItems[0]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTab')),
        true);
    assertEquals(
        menuItems[1]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewWindow')),
        true);
    assertEquals(
        menuItems[2]!.textContent!.includes(
            loadTimeData.getString('menuOpenIncognito')),
        true);
    assertEquals(
        menuItems[3]!.textContent!.includes(
            loadTimeData.getString('menuOpenSplitView')),
        true);
    assertEquals(
        menuItems[4]!.textContent!.includes(loadTimeData.getString('menuEdit')),
        true);
    assertEquals(
        menuItems[5]!.textContent!.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')),
        true);
    assertEquals(
        menuItems[6]!.textContent!.includes(
            loadTimeData.getString('tooltipDelete')),
        true);
  });

  test('ShowsMenuItemsForSingleSelectFolder', async () => {
    const selection = [service.findBookmarkWithId('5')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems = powerBookmarksContextMenu.shadowRoot!.querySelectorAll(
        '.dropdown-item');
    assertEquals(menuItems.length, 7);
    assertEquals(
        menuItems[0]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTab')),
        true);
    assertEquals(
        menuItems[1]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewWindow')),
        true);
    assertEquals(
        menuItems[2]!.textContent!.includes(
            loadTimeData.getString('menuOpenIncognito')),
        true);
    assertEquals(
        menuItems[3]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTabGroup')),
        true);
    assertEquals(
        menuItems[4]!.textContent!.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')),
        true);
    assertEquals(
        menuItems[5]!.textContent!.includes(
            loadTimeData.getString('menuRename')),
        true);
    assertEquals(
        menuItems[6]!.textContent!.includes(
            loadTimeData.getString('tooltipDelete')),
        true);
  });

  test('ShowsMenuItemsForMultiSelect', async () => {
    const selection =
        [service.findBookmarkWithId('3')!, service.findBookmarkWithId('4')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems = powerBookmarksContextMenu.shadowRoot!.querySelectorAll(
        '.dropdown-item');
    assertEquals(menuItems.length, 6);
    assertEquals(
        menuItems[0]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTabWithCount')),
        true);
    assertEquals(
        menuItems[1]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewWindowWithCount')),
        true);
    assertEquals(
        menuItems[2]!.textContent!.includes(
            loadTimeData.getString('menuOpenIncognitoWithCount')),
        true);
    assertEquals(
        menuItems[3]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTabGroupWithCount')),
        true);
    assertEquals(
        menuItems[4]!.textContent!.includes(
            loadTimeData.getString('tooltipMove')),
        true);
    assertEquals(
        menuItems[5]!.textContent!.includes(
            loadTimeData.getString('tooltipDelete')),
        true);
  });

  test('ShowsMenuItemsForPriceTracking', async () => {
    const selection = [service.findBookmarkWithId('4')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, true, true, false);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems = powerBookmarksContextMenu.shadowRoot!.querySelectorAll(
        '.dropdown-item');
    assertEquals(menuItems.length, 8);
    assertEquals(
        menuItems[0]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTab')),
        true);
    assertEquals(
        menuItems[1]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewWindow')),
        true);
    assertEquals(
        menuItems[2]!.textContent!.includes(
            loadTimeData.getString('menuOpenIncognito')),
        true);
    assertEquals(
        menuItems[3]!.textContent!.includes(
            loadTimeData.getString('menuOpenSplitView')),
        true);
    assertEquals(
        menuItems[4]!.textContent!.includes(loadTimeData.getString('menuEdit')),
        true);
    assertEquals(
        menuItems[5]!.textContent!.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')),
        true);
    assertEquals(
        menuItems[6]!.textContent!.includes(
            loadTimeData.getString('menuUntrackPrice')),
        true);
    assertEquals(
        menuItems[7]!.textContent!.includes(
            loadTimeData.getString('tooltipDelete')),
        true);
  });

  test('ShowsMenuItemsForUserWithIncognitoDisabled', async () => {
    loadTimeData.overrideValues({
      isIncognitoModeAvailable: false,
    });

    const selection = [service.findBookmarkWithId('5')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems = powerBookmarksContextMenu.shadowRoot!.querySelectorAll(
        '.dropdown-item');
    assertEquals(menuItems.length, 6);
    assertEquals(
        menuItems[0]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTab')),
        true);
    assertEquals(
        menuItems[1]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewWindow')),
        true);
    assertEquals(
        menuItems[2]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTabGroup')),
        true);
    assertEquals(
        menuItems[3]!.textContent!.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')),
        true);
    assertEquals(
        menuItems[4]!.textContent!.includes(
            loadTimeData.getString('menuRename')),
        true);
    assertEquals(
        menuItems[5]!.textContent!.includes(
            loadTimeData.getString('tooltipDelete')),
        true);
  });

  test('ShowsMenuItemsForUserWithSplitViewDisabled', async () => {
    loadTimeData.overrideValues({
      splitViewEnabled: false,
      isIncognitoModeAvailable: true,
    });

    const selection = [service.findBookmarkWithId('3')!];
    powerBookmarksContextMenu.showAtPosition(
        new MouseEvent('click'), selection, false, false, false);

    await waitAfterNextRender(powerBookmarksContextMenu);

    const menuItems = powerBookmarksContextMenu.shadowRoot!.querySelectorAll(
        '.dropdown-item');
    assertEquals(menuItems.length, 6);
    assertEquals(
        menuItems[0]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewTab')),
        true);
    assertEquals(
        menuItems[1]!.textContent!.includes(
            loadTimeData.getString('menuOpenNewWindow')),
        true);
    assertEquals(
        menuItems[2]!.textContent!.includes(
            loadTimeData.getString('menuOpenIncognito')),
        true);
    assertEquals(
        menuItems[3]!.textContent!.includes(loadTimeData.getString('menuEdit')),
        true);
    assertEquals(
        menuItems[4]!.textContent!.includes(
            loadTimeData.getString('menuMoveToBookmarksBar')),
        true);
    assertEquals(
        menuItems[5]!.textContent!.includes(
            loadTimeData.getString('tooltipDelete')),
        true);
  });
});
