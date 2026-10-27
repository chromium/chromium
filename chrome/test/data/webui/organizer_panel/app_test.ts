// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerPanelAppElement, OrganizerPanelPageRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {browserProxyFactory, foreignTabsBrowserProxyFactory, ForeignTabsPageHandlerRemote, organizerPanelBrowserProxyFactory, OrganizerPanelPageHandlerRemote, PageHandlerRemote, RecentTabsDelegate, tabGroupsBrowserProxyFactory, TabGroupsOrganizerPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
      clearSearch: 'Clear search',
      closeTab: 'Close tab',
      collapse: 'Collapse',
      createTabGroup: 'Create tab group',
      currentlyPlaying: 'Currently playing',
      expand: 'Expand',
      foreignTabsEnabled: false,
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

    const shortcut = app.$.shortcut;
    assertTrue(!!shortcut);
    assertEquals('Ctrl+Shift+A', shortcut.textContent.trim());

    assertTrue(hasStyle(searchField.$.prompt, 'text-overflow', 'ellipsis'));
  });

  test('elides placeholder when there is not enough room', async () => {
    const searchField = app.$.searchField;

    // Set search field width narrow enough that the placeholder cannot fit
    // alongside the shortcut.
    searchField.style.width = '120px';
    await microtasksFinished();

    const prompt = searchField.$.prompt;
    assertTrue(prompt.clientWidth < prompt.scrollWidth);
    assertTrue(hasStyle(prompt, 'text-overflow', 'ellipsis'));
  });

  test(
      'focuses input when magnifying glass icon is clicked while unfocused',
      () => {
        const searchField = app.$.searchField;
        const searchInput = searchField.getSearchInput();
        searchInput.blur();
        assertNotEquals(searchInput, searchField.shadowRoot.activeElement);

        searchField.$.icon.click();
        assertEquals(searchInput, searchField.shadowRoot.activeElement);
      });

  test('focuses input when shortcut is clicked while unfocused', () => {
    const searchField = app.$.searchField;
    const searchInput = searchField.getSearchInput();
    searchInput.blur();
    assertNotEquals(searchInput, searchField.shadowRoot.activeElement);

    app.$.shortcut.click();
    assertEquals(searchInput, searchField.shadowRoot.activeElement);
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

  test(
      'right clicking app prevents default and does not show context menu',
      async () => {
        const event = new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          clientX: 50,
          clientY: 100,
        });
        window.dispatchEvent(event);
        await microtasksFinished();

        assertTrue(event.defaultPrevented);
      });

  test(
      'right clicking item without context menu prevents default', async () => {
        const item = {
          title: ['Open Tab'],
        };
        const listSection =
            app.$.list.shadowRoot.querySelector('organizer-list-section')!;
        listSection.items = [item];
        await microtasksFinished();

        const listItem = listSection.shadowRoot.querySelector(
            'organizer-list-section-item')!;
        const event = new MouseEvent('contextmenu', {
          bubbles: true,
          cancelable: true,
          composed: true,
          clientX: 50,
          clientY: 100,
        });
        listItem.$.crUrlListItem.dispatchEvent(event);
        await microtasksFinished();

        assertTrue(event.defaultPrevented);
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

  test('tab navigation with close button', async () => {
    const closeBtn = document.createElement('button');
    closeBtn.id = 'closeButton';
    closeBtn.tabIndex = 0;
    app.shadowRoot.appendChild(closeBtn);
    await microtasksFinished();

    // Tab on close button focuses search field input
    closeBtn.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    await microtasksFinished();
    assertEquals(app.$.searchField.getSearchInput(), getDeepActiveElement());

    // Shift+Tab on close button focuses last section header
    closeBtn.focus();
    closeBtn.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      shiftKey: true,
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    await microtasksFinished();
    const sections = app.$.list.getVisibleSections();
    const lastSection = sections[sections.length - 1]!;
    assertEquals(
        lastSection.$.header.$.expandButton.$.icon, getDeepActiveElement());
  });

  test('focus-after-list and focus-before-list events from list', async () => {
    // focus-before-list focuses search field
    app.$.list.fire('focus-before-list');
    await microtasksFinished();
    assertEquals(app.$.searchField.getSearchInput(), getDeepActiveElement());

    // Without close button, focus-after-list does not focus search field
    // and does not prevent default.
    const dummyButton = document.createElement('button');
    document.body.appendChild(dummyButton);
    dummyButton.focus();
    assertEquals(dummyButton, getDeepActiveElement());

    const withoutCloseEvent =
        new KeyboardEvent('keydown', {key: 'Tab', cancelable: true});
    app.$.list.fire('focus-after-list', {sourceEvent: withoutCloseEvent});
    await microtasksFinished();
    assertEquals(dummyButton, getDeepActiveElement());
    assertFalse(withoutCloseEvent.defaultPrevented);
    dummyButton.remove();

    // With close button, focus-after-list focuses close button and prevents
    // default.
    const closeBtn = document.createElement('button');
    closeBtn.id = 'closeButton';
    closeBtn.tabIndex = 0;
    app.shadowRoot.appendChild(closeBtn);
    await microtasksFinished();

    const withCloseEvent =
        new KeyboardEvent('keydown', {key: 'Tab', cancelable: true});
    app.$.list.fire('focus-after-list', {sourceEvent: withCloseEvent});
    await microtasksFinished();
    assertEquals(closeBtn, getDeepActiveElement());
    assertTrue(withCloseEvent.defaultPrevented);
  });
});
