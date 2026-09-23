// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerPanelAppElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {browserProxyFactory, PageHandlerRemote, tabGroupsBrowserProxyFactory, TabGroupsOrganizerPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerPanelAppTest', () => {
  let app: OrganizerPanelAppElement;
  let mockPageHandler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let mockTabGroupsHandler: TestMock<TabGroupsOrganizerPageHandlerRemote>&
      TabGroupsOrganizerPageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      cjkWordBoundaryEnabled: false,
      clearSearch: 'Clear search',
      closeTab: 'Close tab',
      noResults: 'No results',
      openTabs: 'Open Tabs',
      recentlyClosed: 'Recently Closed',
      searchTabs: 'Search Tabs',
      shortcutText: 'Ctrl+Shift+A',
      tabGroupMoreOptions: 'More options',
      tabGroups: 'Tab Groups',
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

    mockTabGroupsHandler =
        TestMock.fromClass(TabGroupsOrganizerPageHandlerRemote);
    mockTabGroupsHandler.setResultFor(
        'getTabGroups', Promise.resolve({tabGroups: []}));
    const {instance: tabGroupsInstance} =
        tabGroupsBrowserProxyFactory.createForTest(mockTabGroupsHandler);
    tabGroupsBrowserProxyFactory.setInstance(tabGroupsInstance);

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
    assertEquals(3, app.$.list.sectionDelegates.length);
  });

  test('updates list searchQuery when search field changes', async () => {
    app.$.searchField.setValue('test search');
    await microtasksFinished();
    assertEquals('test search', app.$.list.searchQuery);
  });

  test(
      'search field remains fixed and list scrolls when overflowing',
      async () => {
        const appStyle = window.getComputedStyle(app);
        assertEquals('flex', appStyle.display);
        assertEquals('column', appStyle.flexDirection);

        const searchFieldStyle = window.getComputedStyle(app.$.searchField);
        assertEquals('0', searchFieldStyle.flexShrink);

        const listStyle = window.getComputedStyle(app.$.list);
        assertEquals('1', listStyle.flexGrow);
        assertEquals('auto', listStyle.overflowY);
        assertEquals('hidden', listStyle.overflowX);
        assertEquals('none', listStyle.overscrollBehavior);
        assertEquals('8px', listStyle.paddingLeft);
        assertEquals('8px', listStyle.paddingRight);

        app.style.height = '100px';
        const dummy = document.createElement('div');
        dummy.style.height = '500px';
        app.$.list.shadowRoot.appendChild(dummy);
        await microtasksFinished();

        assertTrue(app.$.list.scrollHeight > app.$.list.clientHeight);

        const searchFieldRectBefore = app.$.searchField.getBoundingClientRect();
        app.$.list.scrollTop = 50;
        assertEquals(50, app.$.list.scrollTop);

        const searchFieldRectAfter = app.$.searchField.getBoundingClientRect();
        assertEquals(searchFieldRectBefore.top, searchFieldRectAfter.top);
      });
});
