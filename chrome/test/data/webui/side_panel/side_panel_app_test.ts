// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://read-later.top-chrome/app.js';

import {LOCAL_STORAGE_TAB_ID_KEY, SidePanelAppElement} from 'chrome://read-later.top-chrome/app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('SidePanelAppElementTest', () => {
  let sidePanelApp: SidePanelAppElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sidePanelApp = document.createElement('side-panel-app');
    document.body.appendChild(sidePanelApp);
  });

  test('RemembersLastActiveTab', () => {
    const tabs = sidePanelApp.shadowRoot!.querySelector('cr-tabs')!;

    // Remove the app, change localStorage to select the Bookmarks tab, and add
    // the app back to the DOM to see if the app changes tabs on
    // connectedCallback.
    sidePanelApp.remove();
    window.localStorage[LOCAL_STORAGE_TAB_ID_KEY] = 'bookmarks';
    document.body.appendChild(sidePanelApp);
    assertEquals(1, tabs.selected);

    // Change the selected tab to Reading List and confirm localStorage has
    // changed.
    tabs.selected = 0;
    assertEquals('readingList', window.localStorage[LOCAL_STORAGE_TAB_ID_KEY]);
  });

  test('LazilyLoadsTabContents', async () => {
    const tabs = sidePanelApp.shadowRoot!.querySelector('cr-tabs')!;
    tabs.selected = 0;
    await flushTasks();
    assertEquals(
        1,
        sidePanelApp.shadowRoot!.querySelectorAll('reading-list-app').length);
    assertEquals(
        0, sidePanelApp.shadowRoot!.querySelectorAll('bookmarks-list').length);

    tabs.selected = 1;
    await flushTasks();
    assertEquals(
        1, sidePanelApp.shadowRoot!.querySelectorAll('bookmarks-list').length);
  });

  test('ForceShowBookmarkTab', async () => {
    const tabs = sidePanelApp.shadowRoot!.querySelector('cr-tabs')!;

    // Remove the app, change localStorage to select the ReadList tab while
    // force showing Bookmarks tab, and add the app back to the DOM to see if
    // the app changes tabs on connectedCallback.
    sidePanelApp.remove();
    window.localStorage[LOCAL_STORAGE_TAB_ID_KEY] = 'readingList';
    loadTimeData.overrideValues({
      shouldShowBookmark: true,
    });

    document.body.appendChild(sidePanelApp);
    assertEquals(1, tabs.selected);
  });
});
