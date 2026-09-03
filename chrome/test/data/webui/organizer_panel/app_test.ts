// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerPanelAppElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {browserProxyFactory, PageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerPanelAppTest', () => {
  let app: OrganizerPanelAppElement;
  let mockPageHandler: PageHandlerRemote&TestMock<PageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      clearSearch: 'Clear search',
      closeTab: 'Close tab',
      openTabs: 'Open Tabs',
      recentlyClosed: 'Recently Closed',
      searchTabs: 'Search Tabs',
      shortcutText: 'Ctrl+Shift+A',
    });
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(mockPageHandler);
    browserProxyFactory.setInstance(instance);
    mockPageHandler.setResultFor('getProfileData', Promise.resolve({
      profileData: {
        windows: [],
        recentlyClosedTabs: [],
        recentlyClosedTabGroups: [],
        recentlyClosedSplitViews: [],
        recentlyClosedSectionExpanded: false,
        tabGroups: [],
      },
    }));

    app = document.createElement('organizer-panel-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('renders search field with correct label and shortcut', () => {
    const searchField = app.$.searchField;
    assertEquals('Search Tabs', searchField.getAttribute('label'));
    assertEquals('Clear search', searchField.getAttribute('clear-label'));

    const shortcut = searchField.querySelector('#shortcut');
    assertTrue(!!shortcut);
    assertEquals('Ctrl+Shift+A', shortcut.textContent);
  });

  test('renders organizer list with expected sections', () => {
    assertEquals(2, app.$.list.sectionDelegates.length);
  });

  test('updates list searchQuery when search field changes', async () => {
    app.$.searchField.setValue('test search');
    await microtasksFinished();
    assertEquals('test search', app.$.list.searchQuery);
  });
});
