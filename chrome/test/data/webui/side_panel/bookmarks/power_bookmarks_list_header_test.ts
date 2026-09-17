// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list_header.js';

import {SortOrder} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {PowerBookmarksListHeaderElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list_header.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestPowerBookmarksDelegate} from './test_power_bookmarks_delegate.js';

suite('SidePanelPowerBookmarksListHeaderTest', () => {
  let header: PowerBookmarksListHeaderElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let delegate: TestPowerBookmarksDelegate;
  let service: PowerBookmarksService;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    delegate = new TestPowerBookmarksDelegate();
    service = new PowerBookmarksService(delegate);
    PowerBookmarksService.setInstance(service);

    loadTimeData.overrideValues({
      sortOrder: SortOrder.kNewest,
    });

    header = document.createElement('power-bookmarks-list-header');
    document.body.appendChild(header);
  });

  test('SortMenuClosesOnFocusout', async () => {
    // Open sort menu.
    const sortButton =
        header.shadowRoot.querySelector<HTMLElement>('.sort-menu-button');
    assertTrue(!!sortButton);
    sortButton.click();
    await microtasksFinished();

    const sortMenu = header.$.sortMenu;
    assertTrue(sortMenu.open);

    // Simulate blur by dispatching focusout event with relatedTarget outside
    // the menu.
    const event = new FocusEvent('focusout', {
      relatedTarget: document.body,
    });
    sortMenu.dispatchEvent(event);

    await microtasksFinished();

    assertFalse(sortMenu.open);
  });
});
