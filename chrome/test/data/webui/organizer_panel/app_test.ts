// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerPanelAppElement, OrganizerPanelPageRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {browserProxyFactory, foreignTabsBrowserProxyFactory, ForeignTabsPageHandlerRemote, organizerPanelBrowserProxyFactory, OrganizerPanelPageHandlerRemote, PageHandlerRemote, RecentTabsDelegate, tabGroupsBrowserProxyFactory, TabGroupsOrganizerPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerPanelAppTest', () => {
  let app: OrganizerPanelAppElement;
  let mockOrganizerPanelHandler: TestMock<OrganizerPanelPageHandlerRemote>&
      OrganizerPanelPageHandlerRemote;
  let remotePage: OrganizerPanelPageRemote;
  let mockPageHandler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let mockTabGroupsHandler: TestMock<TabGroupsOrganizerPageHandlerRemote>&
      TabGroupsOrganizerPageHandlerRemote;
  let mockForeignTabsHandler: TestMock<ForeignTabsPageHandlerRemote>&
      ForeignTabsPageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      cjkWordBoundaryEnabled: false,
      clearSearch: 'Clear search',
      closeTab: 'Close tab',
      closeTabSearch: isMac ? 'Close Tab Search' : 'Close tab search',
      collapse: 'Collapse',
      collapseAll: isMac ? 'Collapse All' : 'Collapse all',
      createTabGroup: 'Create tab group',
      currentlyPlaying: 'Currently playing',
      expand: 'Expand',
      expandAll: isMac ? 'Expand All' : 'Expand all',
      foreignTabsEnabled: false,
      goBackToOpenTabs: isMac ? 'Go Back to Open Tabs' : 'Go back to open tabs',
      isIncognitoMode: false,
      noRecentlyClosedTabs: 'No recently closed tabs',
      noResults: 'No results',
      options: 'Options',
      recentlyClosed: 'Recently Closed',
      recentlyVisited: 'Recently visited',
      searchTabs: 'Search Tabs',
      shortcutText: 'Ctrl+Shift+A',
      tabGroupMoreOptions: 'More options',
      tabGroups: 'Tab Groups',
    });
    mockOrganizerPanelHandler =
        TestMock.fromClass(OrganizerPanelPageHandlerRemote);
    mockOrganizerPanelHandler.setPromiseResolveFor(
        'getSectionState', {state: {expanded: false, showAll: false}});
    const {instance: organizerPanelInstance, remote} =
        organizerPanelBrowserProxyFactory.createForTest(
            mockOrganizerPanelHandler);
    organizerPanelBrowserProxyFactory.setInstance(organizerPanelInstance);
    remotePage = remote;

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

    mockForeignTabsHandler = TestMock.fromClass(ForeignTabsPageHandlerRemote);
    mockForeignTabsHandler.setResultFor(
        'getForeignTabs', Promise.resolve({tabs: []}));
    foreignTabsBrowserProxyFactory.setInstance(
        {handler: mockForeignTabsHandler});

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
    assertTrue(app.$.list.sectionDelegates.some(
        delegate => delegate instanceof RecentTabsDelegate));
  });

  test('hides recent tabs section in incognito mode', async () => {
    loadTimeData.overrideValues({isIncognitoMode: true});
    const incognitoApp = document.createElement('organizer-panel-app');
    document.body.appendChild(incognitoApp);
    await microtasksFinished();

    assertEquals(2, incognitoApp.$.list.sectionDelegates.length);
    assertTrue(incognitoApp.$.list.sectionDelegates.every(
        delegate => !(delegate instanceof RecentTabsDelegate)));
  });

  test('renders foreign tabs section when flag is enabled', async () => {
    loadTimeData.overrideValues({
      tabsOnOtherDevices: 'Tabs on other devices',
      foreignTabsEnabled: true,
    });
    const testApp = document.createElement('organizer-panel-app');
    document.body.appendChild(testApp);
    await microtasksFinished();

    assertEquals(4, testApp.$.list.sectionDelegates.length);
    assertEquals(
        'Tabs on other devices',
        testApp.$.list.sectionDelegates[3]!.getHeader());
  });

  test('updates list searchQuery when search field changes', async () => {
    app.$.searchField.setValue('test search');
    await microtasksFinished();
    assertEquals('test search', app.$.list.searchQuery);
  });

  test('updates divider color when vertical tabs enabled', async () => {
    app.style.setProperty('--color-sys-divider', 'rgb(10, 20, 30)');
    app.style.setProperty('--color-sys-on-header-divider', 'rgb(40, 50, 60)');
    await microtasksFinished();

    const divider =
        app.$.list.shadowRoot.querySelector<HTMLElement>('.divider');
    assertTrue(!!divider);
    assertFalse(app.verticalTabsEnabled);
    assertFalse(app.hasAttribute('vertical-tabs-enabled'));
    assertEquals('rgb(10, 20, 30)', getComputedStyle(divider).backgroundColor);

    remotePage.onVerticalTabsEnabledChanged(true);
    await microtasksFinished();

    assertTrue(app.verticalTabsEnabled);
    assertTrue(app.hasAttribute('vertical-tabs-enabled'));
    assertEquals('rgb(40, 50, 60)', getComputedStyle(divider).backgroundColor);

    remotePage.onVerticalTabsEnabledChanged(false);
    await microtasksFinished();

    assertFalse(app.verticalTabsEnabled);
    assertFalse(app.hasAttribute('vertical-tabs-enabled'));
    assertEquals('rgb(10, 20, 30)', getComputedStyle(divider).backgroundColor);
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
        assertEquals('0', listStyle.flexGrow);
        assertEquals('auto', listStyle.overflowY);
        assertEquals('hidden', listStyle.overflowX);
        assertEquals('none', listStyle.overscrollBehavior);
        assertEquals('8px', listStyle.paddingLeft);
        assertEquals('0px', listStyle.paddingRight);
        assertEquals('stable', listStyle.scrollbarGutter);

        const dragRegionStyle = window.getComputedStyle(app.$.dragRegion);
        assertEquals('1', dragRegionStyle.flexGrow);

        app.style.height = '100px';
        app.style.width = '300px';
        const clientWidthBefore = app.$.list.clientWidth;
        const dummy = document.createElement('div');
        dummy.style.height = '500px';
        app.$.list.shadowRoot.appendChild(dummy);
        await microtasksFinished();

        assertTrue(app.$.list.scrollHeight > app.$.list.clientHeight);
        assertEquals(clientWidthBefore, app.$.list.clientWidth);

        const searchFieldRectBefore = app.$.searchField.getBoundingClientRect();
        app.$.list.scrollTop = 50;
        assertEquals(50, app.$.list.scrollTop);

        const searchFieldRectAfter = app.$.searchField.getBoundingClientRect();
        assertEquals(searchFieldRectBefore.top, searchFieldRectAfter.top);
      });

  test('drag region expands to fill unused height', async () => {
    app.style.height = '500px';
    await microtasksFinished();

    const dragRegionRect = app.$.dragRegion.getBoundingClientRect();
    const listRect = app.$.list.getBoundingClientRect();
    const appRect = app.getBoundingClientRect();

    assertTrue(dragRegionRect.height > 0);
    assertEquals(appRect.bottom, dragRegionRect.bottom);
    assertEquals(listRect.bottom, dragRegionRect.top);
  });

  test('right clicking app shows context menu', async () => {
    assertFalse(app.$.menu.open);

    const event = new MouseEvent('contextmenu', {
      bubbles: true,
      cancelable: true,
      clientX: 50,
      clientY: 100,
    });
    app.dispatchEvent(event);
    await microtasksFinished();

    assertTrue(event.defaultPrevented);
    assertTrue(app.$.menu.open);

    const expandAllButton =
        app.$.menu.querySelector<HTMLButtonElement>('#expandAllButton');
    assertTrue(!!expandAllButton);
    assertEquals(
        isMac ? 'Expand All' : 'Expand all',
        expandAllButton.textContent.trim());

    const closeButton =
        app.$.menu.querySelector<HTMLButtonElement>('#closeTabSearchButton');
    assertTrue(!!closeButton);
    assertEquals(
        isMac ? 'Close Tab Search' : 'Close tab search',
        closeButton.textContent.trim());

    remotePage.onVerticalTabsEnabledChanged(true);
    await microtasksFinished();
    assertEquals(
        isMac ? 'Go Back to Open Tabs' : 'Go back to open tabs',
        closeButton.textContent.trim());

    remotePage.onVerticalTabsEnabledChanged(false);
    await microtasksFinished();
    assertEquals(
        isMac ? 'Close Tab Search' : 'Close tab search',
        closeButton.textContent.trim());
  });

  test('right clicking drag region does not show menu', async () => {
    const event = new MouseEvent('contextmenu', {
      bubbles: true,
      cancelable: true,
      composed: true,
      clientX: 50,
      clientY: 100,
    });
    app.$.dragRegion.dispatchEvent(event);
    await microtasksFinished();

    assertTrue(event.defaultPrevented);
    assertFalse(app.$.menu.open);
  });

  test(
      'clicking expand all in context menu calls expandAll on list',
      async () => {
        let expandAllCalled = false;
        app.$.list.expandAll = () => {
          expandAllCalled = true;
        };

        app.dispatchEvent(new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          clientX: 50,
          clientY: 100,
        }));
        await microtasksFinished();
        assertTrue(app.$.menu.open);

        const expandAllButton =
            app.$.menu.querySelector<HTMLButtonElement>('#expandAllButton')!;
        expandAllButton.click();
        await microtasksFinished();

        assertTrue(expandAllCalled);
        assertFalse(app.$.menu.open);
      });

  test(
      'shows collapse all in context menu when all sections are expanded and ' +
          'clicking it calls collapseAll on list',
      async () => {
        app.$.list.expandAll();
        await microtasksFinished();

        let collapseAllCalled = false;
        app.$.list.collapseAll = () => {
          collapseAllCalled = true;
        };

        app.dispatchEvent(new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          clientX: 50,
          clientY: 100,
        }));
        await microtasksFinished();
        assertTrue(app.$.menu.open);

        assertEquals(
            null,
            app.$.menu.querySelector<HTMLButtonElement>('#expandAllButton'));
        const collapseAllButton =
            app.$.menu.querySelector<HTMLButtonElement>('#collapseAllButton');
        assertTrue(!!collapseAllButton);
        assertEquals(
            isMac ? 'Collapse All' : 'Collapse all',
            collapseAllButton.textContent.trim());

        collapseAllButton.click();
        await microtasksFinished();

        assertTrue(collapseAllCalled);
        assertFalse(app.$.menu.open);
      });

  test(
      'disables expand and collapse all in context menu when search is active',
      async () => {
        const expandAllButton =
            app.$.menu.querySelector<HTMLButtonElement>('#expandAllButton')!;
        assertFalse(expandAllButton.disabled);

        app.$.searchField.setValue('test search');
        await microtasksFinished();
        assertTrue(expandAllButton.disabled);

        app.$.searchField.setValue('');
        await microtasksFinished();
        assertFalse(expandAllButton.disabled);

        app.$.list.expandAll();
        app.dispatchEvent(new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          clientX: 50,
          clientY: 100,
        }));
        await microtasksFinished();

        const collapseAllButton =
            app.$.menu.querySelector<HTMLButtonElement>('#collapseAllButton')!;
        assertFalse(collapseAllButton.disabled);

        app.$.searchField.setValue('test search');
        await microtasksFinished();
        assertTrue(collapseAllButton.disabled);

        app.$.searchField.setValue('');
        await microtasksFinished();
        assertFalse(collapseAllButton.disabled);
      });

  test(
      'clicking close tab search in context menu calls closePanel',
      async () => {
        app.dispatchEvent(new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          clientX: 50,
          clientY: 100,
        }));
        await microtasksFinished();
        assertTrue(app.$.menu.open);

        const closeButton = app.$.menu.querySelector<HTMLButtonElement>(
            '#closeTabSearchButton')!;
        closeButton.click();
        await microtasksFinished();

        assertEquals(1, mockOrganizerPanelHandler.getCallCount('closePanel'));
        assertFalse(app.$.menu.open);
      });

  test('captured contextmenu does not show app context menu', async () => {
    assertFalse(app.$.menu.open);

    const child = document.createElement('div');
    app.appendChild(child);
    child.addEventListener('contextmenu', (e) => {
      e.stopPropagation();
    });

    child.dispatchEvent(new MouseEvent('contextmenu', {
      bubbles: true,
      cancelable: true,
      clientX: 50,
      clientY: 100,
    }));
    await microtasksFinished();

    assertFalse(app.$.menu.open);
  });

  test('resets search query and scroll position on hidden', async () => {
    app.style.height = '100px';
    app.style.width = '300px';
    const dummy = document.createElement('div');
    dummy.style.height = '500px';
    app.$.list.shadowRoot.appendChild(dummy);
    app.$.searchField.setValue('test search');
    await microtasksFinished();

    app.$.list.scrollTop = 50;
    assertEquals('test search', app.$.searchField.getValue());
    assertEquals('test search', app.$.list.searchQuery);
    assertEquals(50, app.$.list.scrollTop);

    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();

    assertEquals('', app.$.searchField.getValue());
    assertEquals('', app.$.list.searchQuery);
    assertEquals(0, app.$.list.scrollTop);

    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();

    assertEquals('', app.$.searchField.getValue());
    assertEquals('', app.$.list.searchQuery);
    assertEquals(0, app.$.list.scrollTop);
  });
});
