// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {ForeignTab, OrganizerPanelAppElement, OrganizerPanelPageRemote, RecentlyClosedSplitView, RecentlyClosedTab, RecentlyClosedTabGroup, Tab, TabGroup} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {browserProxyFactory, foreignTabsBrowserProxyFactory, ForeignTabsDelegate, ForeignTabsPageHandlerRemote, OpenTabsDelegate, OpenTabsItemType, organizerPanelBrowserProxyFactory, OrganizerPanelCloseAction, OrganizerPanelPageHandlerRemote, PageHandlerRemote, RecentTabsDelegate, SearchApiProxyImpl, tabGroupsBrowserProxyFactory, TabGroupsDelegate, TabGroupsOrganizerPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchApiProxy} from './test_search_api_proxy.js';

suite('OrganizerPanelAppTest', () => {
  let app: OrganizerPanelAppElement;
  let metrics: MetricsTracker;
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
    metrics = fakeMetricsPrivate();
    loadTimeData.resetForTesting({
      audioMuted: 'Audio muted',
      clearSearch: 'Clear search',
      close: 'Close',
      closeSplitView: 'Close split view: $1, $2',
      closeTab: 'Close $1',
      collapse: 'Collapse',
      createTabGroup: 'Create tab group',
      currentlyPlaying: 'Currently playing',
      diceSupportEnabled: true,
      expand: 'Expand',
      foreignTabsEnabled: false,
      isIncognitoMode: false,
      isUserSignedIn: true,
      noRecentlyClosedTabs: 'No recently closed tabs',
      noResults: 'No results',
      openTabGroup: 'Open tab group',
      options: 'Options',
      recentlyClosed: 'Recently closed',
      recentlyVisited: 'Recently visited',
      savedTabGroup: 'Saved tab group',
      searchTabs: 'Search Tabs',
      sectionOptions: 'Options for $1',
      shortcutText: 'Ctrl+Shift+A',
      signInOnPhone: 'Sign in on your phone',
      splitView: 'Split view',
      tabGroup: 'Tab group',
      tabGroupMoreOptions: 'More options',
      tabGroupMoreOptionsWithTitle: 'More options for $1',
      tabGroups: 'Tab groups',
      tabsOnOtherDevices: 'Tabs on other devices',
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
    loadTimeData.overrideValues({foreignTabsEnabled: true});
    const testApp = document.createElement('organizer-panel-app');
    document.body.appendChild(testApp);
    await microtasksFinished();

    assertEquals(4, testApp.$.list.sectionDelegates.length);
    assertEquals(
        'Tabs on other devices',
        testApp.$.list.sectionDelegates[3]!.getHeader());
    const sections =
        testApp.$.list.shadowRoot.querySelectorAll('organizer-list-section');
    assertEquals(4, sections.length);
    assertFalse(sections[3]!.hidden);
    assertEquals(4, testApp.$.list.getVisibleSections().length);
  });

  test('hides foreign tabs section when user is signed out', async () => {
    loadTimeData.overrideValues({
      foreignTabsEnabled: true,
      isUserSignedIn: false,
    });
    const testApp = document.createElement('organizer-panel-app');
    document.body.appendChild(testApp);
    await microtasksFinished();

    const sections =
        testApp.$.list.shadowRoot.querySelectorAll('organizer-list-section');
    assertEquals(4, sections.length);
    assertTrue(sections[3]!.hidden);
    assertEquals(3, testApp.$.list.getVisibleSections().length);
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

  test('clicking close button closes panel', async () => {
    const closeBtn = app.shadowRoot.querySelector<HTMLElement>('#closeButton')!;
    assertTrue(!!closeBtn);
    assertEquals(0, mockOrganizerPanelHandler.getCallCount('closePanel'));

    closeBtn.click();
    await microtasksFinished();
    assertEquals(1, mockOrganizerPanelHandler.getCallCount('closePanel'));

    remotePage.onIsInTrayViewChanged(false);
    await microtasksFinished();
    assertFalse(app.inTrayView);
    assertEquals(null, app.shadowRoot.querySelector('#closeButton'));

    remotePage.onIsInTrayViewChanged(true);
    await microtasksFinished();
    assertTrue(app.inTrayView);
    assertTrue(!!app.shadowRoot.querySelector('#closeButton'));
  });

  test('tab navigation with close button', async () => {
    const closeBtn = app.shadowRoot.querySelector<HTMLElement>('#closeButton')!;
    assertTrue(!!closeBtn);

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

    // Shift+Tab on close button focuses last section menu button when expanded
    lastSection.expanded = true;
    await microtasksFinished();
    closeBtn.focus();
    closeBtn.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Tab',
      shiftKey: true,
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    await microtasksFinished();
    assertEquals(lastSection.$.header.$.menuButton, getDeepActiveElement());
  });

  test('focus-after-list and focus-before-list events from list', async () => {
    // focus-before-list focuses search field
    app.$.list.fire('focus-before-list');
    await microtasksFinished();
    assertEquals(app.$.searchField.getSearchInput(), getDeepActiveElement());

    // When not in tray view (without close button), focus-after-list does
    // not focus search field and does not prevent default.
    app.inTrayView = false;
    await microtasksFinished();

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

    // When in tray view (with close button), focus-after-list focuses
    // close button and prevents default.
    app.inTrayView = true;
    await microtasksFinished();

    const closeBtn = app.shadowRoot.querySelector<HTMLElement>('#closeButton')!;
    assertTrue(!!closeBtn);

    const withCloseEvent =
        new KeyboardEvent('keydown', {key: 'Tab', cancelable: true});
    app.$.list.fire('focus-after-list', {sourceEvent: withCloseEvent});
    await microtasksFinished();
    assertEquals(closeBtn, getDeepActiveElement());
    assertTrue(withCloseEvent.defaultPrevented);
  });

  test(
      'tabbing from last header menu button loops to close button or exits',
      async () => {
        const sections = app.$.list.getVisibleSections();
        const lastSection = sections[sections.length - 1]!;
        lastSection.expanded = true;
        await microtasksFinished();

        lastSection.$.header.focusMenuButton();
        await microtasksFinished();
        assertEquals(lastSection.$.header.$.menuButton, getDeepActiveElement());

        // When not in tray view (without close button): Tab forwards
        // focus-after-list without preventDefault, and menuButton follows
        // expandButton in DOM order so focus is not trapped within the header.
        app.inTrayView = false;
        await microtasksFinished();

        lastSection.$.header.focusMenuButton();
        await microtasksFinished();

        const withoutCloseEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          bubbles: true,
          composed: true,
          cancelable: true,
        });
        lastSection.$.header.$.menuButton.dispatchEvent(withoutCloseEvent);
        await microtasksFinished();
        assertFalse(withoutCloseEvent.defaultPrevented);
        assertTrue(Boolean(
            lastSection.$.header.$.expandButton.compareDocumentPosition(
                lastSection.$.header.$.menuButton) &
            Node.DOCUMENT_POSITION_FOLLOWING));

        // When in tray view (with close button): Tab focuses close
        // button and prevents default.
        app.inTrayView = true;
        await microtasksFinished();

        const closeBtn =
            app.shadowRoot.querySelector<HTMLElement>('#closeButton')!;
        assertTrue(!!closeBtn);

        lastSection.$.header.focusMenuButton();
        await microtasksFinished();
        assertEquals(lastSection.$.header.$.menuButton, getDeepActiveElement());

        const withCloseEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          bubbles: true,
          composed: true,
          cancelable: true,
        });
        lastSection.$.header.$.menuButton.dispatchEvent(withCloseEvent);
        await microtasksFinished();
        assertEquals(closeBtn, getDeepActiveElement());
        assertTrue(withCloseEvent.defaultPrevented);

        // Shift+Tab from close button focuses last section menu button.
        const reverseEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          shiftKey: true,
          bubbles: true,
          composed: true,
          cancelable: true,
        });
        closeBtn.dispatchEvent(reverseEvent);
        await microtasksFinished();
        assertEquals(lastSection.$.header.$.menuButton, getDeepActiveElement());
        assertTrue(reverseEvent.defaultPrevented);
      });

  test('tab navigation from search field to list', async () => {
    SearchApiProxyImpl.setInstance(new TestSearchApiProxy());
    const sections = app.$.list.getVisibleSections();
    sections[0]!.items = [{title: ['Open Tab']}];
    await microtasksFinished();

    const searchInput = app.$.searchField.getSearchInput();
    app.$.searchField.setValue('Tab');
    await microtasksFinished();

    assertTrue(app.$.searchField.hideClearButton);
    assertEquals(
        null, app.$.searchField.shadowRoot.querySelector('#clearSearch'));

    // Tab on searchInput focuses the first section header directly.
    searchInput.focus();
    const inputTabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      composed: true,
      cancelable: true,
    });
    searchInput.dispatchEvent(inputTabEvent);
    await microtasksFinished();
    assertTrue(inputTabEvent.defaultPrevented);
    assertEquals(sections[0]!.$.header, getDeepActiveElement());
  });

  test('records user actions when delegates are interacted with', () => {
    const openTabsDelegate =
        app.$.list.sectionDelegates.find(d => d instanceof OpenTabsDelegate) as
        OpenTabsDelegate;
    assertTrue(!!openTabsDelegate);

    // SwitchToTab and CloseTab
    const dummyTabItem = {
      title: ['Open Tab'],
      data: {
        type: OpenTabsItemType.TAB as const,
        tab: {tabId: 1, alertStates: []} as unknown as Tab,
      },
    };
    openTabsDelegate.onItemClick(dummyTabItem);
    assertEquals(1, metrics.count('OrganizerPanel.SwitchToTab'));

    openTabsDelegate.onItemActionButtonClicked(dummyTabItem);
    assertEquals(1, metrics.count('OrganizerPanel.CloseTab'));

    // RecentTabsDelegate: OpenRecentlyClosedTab, OpenRecentlyClosedTabGroup,
    // OpenRecentlyClosedSplitView
    const recentTabsDelegate =
        app.$.list.sectionDelegates.find(
            d => d instanceof RecentTabsDelegate) as RecentTabsDelegate;
    assertTrue(!!recentTabsDelegate);

    const recentTabItem = {
      title: ['Recent Tab'],
      data: {tabId: 10} as unknown as RecentlyClosedTab,
    };
    recentTabsDelegate.onItemClick(recentTabItem);
    assertEquals(1, metrics.count('OrganizerPanel.OpenRecentlyClosedTab'));

    const recentTabGroupItem = {
      title: ['Recent Group'],
      data: {sessionId: 20, color: 1} as unknown as RecentlyClosedTabGroup,
    };
    recentTabsDelegate.onItemClick(recentTabGroupItem);
    assertEquals(1, metrics.count('OrganizerPanel.OpenRecentlyClosedTabGroup'));

    const recentSplitItem = {
      title: ['Recent Split'],
      data: {sessionId: 30, tabUrls: []} as unknown as RecentlyClosedSplitView,
    };
    recentTabsDelegate.onItemClick(recentSplitItem);
    assertEquals(
        1, metrics.count('OrganizerPanel.OpenRecentlyClosedSplitView'));

    // TabGroupsDelegate: OpenTabGroup and CreateTabGroup
    const tabGroupsDelegate =
        app.$.list.sectionDelegates.find(d => d instanceof TabGroupsDelegate) as
        TabGroupsDelegate;
    assertTrue(!!tabGroupsDelegate);

    const groupItem = {
      title: ['Tab Group'],
      data: {id: 'group-1'} as TabGroup,
    };
    tabGroupsDelegate.onItemClick(groupItem);
    assertEquals(1, metrics.count('OrganizerPanel.OpenTabGroup'));

    // Zero state "+ Create tab group"
    const zeroState = tabGroupsDelegate.getZeroState();
    const container = document.createElement('div');
    render(zeroState, container);
    const zeroStateItem =
        container.querySelector('organizer-list-section-item')!;
    assertTrue(!!zeroStateItem);
    zeroStateItem.click();
    assertEquals(1, metrics.count('OrganizerPanel.CreateTabGroup'));

    // ForeignTabsDelegate: OpenCrossDeviceTab
    const foreignTabsDelegate = new ForeignTabsDelegate();
    const foreignTabItem = {
      title: ['Foreign Tab'],
      data: {sessionTag: 'tag', sessionTabId: 40} as ForeignTab,
    };
    foreignTabsDelegate.onItemClick(foreignTabItem);
    assertEquals(1, metrics.count('OrganizerPanel.OpenCrossDeviceTab'));
  });

  test('records close action metric on hidden', () => {
    const triggerClose = () => {
      Object.defineProperty(
          document, 'visibilityState', {value: 'hidden', writable: true});
      document.dispatchEvent(new Event('visibilitychange'));
      Object.defineProperty(
          document, 'visibilityState', {value: 'visible', writable: true});
      document.dispatchEvent(new Event('visibilitychange'));
    };

    // Records NO_ACTION when closed without interaction.
    triggerClose();
    assertEquals(
        1,
        metrics.count(
            'Tabs.Organizer.CloseAction', OrganizerPanelCloseAction.NO_ACTION));

    const openTabsDelegate =
        app.$.list.sectionDelegates.find(d => d instanceof OpenTabsDelegate) as
        OpenTabsDelegate;
    assertTrue(!!openTabsDelegate);

    const dummyTabItem = {
      title: ['Open Tab'],
      data: {
        type: OpenTabsItemType.TAB as const,
        tab: {tabId: 1, alertStates: []} as unknown as Tab,
      },
    };

    // Switching to a tab records SWITCH_TO_TAB.
    openTabsDelegate.onItemClick(dummyTabItem);
    triggerClose();
    assertEquals(
        1,
        metrics.count(
            'Tabs.Organizer.CloseAction',
            OrganizerPanelCloseAction.SWITCH_TO_TAB));

    // Closing a tab and then closing the panel records CLOSED_TAB.
    openTabsDelegate.onItemActionButtonClicked(dummyTabItem);
    triggerClose();
    assertEquals(
        1,
        metrics.count(
            'Tabs.Organizer.CloseAction',
            OrganizerPanelCloseAction.CLOSED_TAB));

    // Resets to NO_ACTION on subsequent close without action.
    triggerClose();
    assertEquals(
        2,
        metrics.count(
            'Tabs.Organizer.CloseAction', OrganizerPanelCloseAction.NO_ACTION));

    // Closing a tab followed by switching to a tab records SWITCH_TO_TAB.
    openTabsDelegate.onItemActionButtonClicked(dummyTabItem);
    openTabsDelegate.onItemClick(dummyTabItem);
    triggerClose();
    assertEquals(
        2,
        metrics.count(
            'Tabs.Organizer.CloseAction',
            OrganizerPanelCloseAction.SWITCH_TO_TAB));

    const tabGroupsDelegate =
        app.$.list.sectionDelegates.find(d => d instanceof TabGroupsDelegate) as
        TabGroupsDelegate;
    assertTrue(!!tabGroupsDelegate);
    mockTabGroupsHandler.setResultFor('showContextMenu', Promise.resolve());

    const dummyGroupItem = {
      title: ['Tab Group'],
      data: {id: 'group-1'} as TabGroup,
    };

    // Opening the tab group 3-dot menu and closing the panel records
    // OPEN_TAB_GROUP_MENU.
    tabGroupsDelegate.onItemActionButtonClicked(
        dummyGroupItem, document.createElement('button'));
    triggerClose();
    assertEquals(
        1,
        metrics.count(
            'Tabs.Organizer.CloseAction',
            OrganizerPanelCloseAction.OPEN_TAB_GROUP_MENU));

    // Opening the tab group context menu via right-click also records
    // OPEN_TAB_GROUP_MENU.
    tabGroupsDelegate.onItemContextMenuClicked(dummyGroupItem, 0, 0);
    triggerClose();
    assertEquals(
        2,
        metrics.count(
            'Tabs.Organizer.CloseAction',
            OrganizerPanelCloseAction.OPEN_TAB_GROUP_MENU));

    // Opening the tab group menu followed by closing a tab records CLOSED_TAB.
    tabGroupsDelegate.onItemActionButtonClicked(
        dummyGroupItem, document.createElement('button'));
    openTabsDelegate.onItemActionButtonClicked(dummyTabItem);
    triggerClose();
    assertEquals(
        2,
        metrics.count(
            'Tabs.Organizer.CloseAction',
            OrganizerPanelCloseAction.CLOSED_TAB));

    // Clicking the cross-device sign-in promo zero state records
    // OPEN_CROSS_DEVICE_PROMO.
    const foreignTabsDelegate = new ForeignTabsDelegate();
    const promoContainer = document.createElement('div');
    render(foreignTabsDelegate.getZeroState(), promoContainer);
    const promoItem =
        promoContainer.querySelector('organizer-list-section-item')!;
    assertTrue(!!promoItem);
    promoItem.click();
    triggerClose();
    assertEquals(
        1,
        metrics.count(
            'Tabs.Organizer.CloseAction',
            OrganizerPanelCloseAction.OPEN_CROSS_DEVICE_PROMO));
  });

  test('records num tabs closed per instance on hidden', () => {
    const triggerClose = () => {
      Object.defineProperty(
          document, 'visibilityState', {value: 'hidden', writable: true});
      document.dispatchEvent(new Event('visibilitychange'));
      Object.defineProperty(
          document, 'visibilityState', {value: 'visible', writable: true});
      document.dispatchEvent(new Event('visibilitychange'));
    };

    // Records 0 when closed without closing any tabs.
    triggerClose();
    assertEquals(
        1, metrics.count('Tabs.Organizer.NumTabsClosedPerInstance', 0));

    const openTabsDelegate =
        app.$.list.sectionDelegates.find(d => d instanceof OpenTabsDelegate) as
        OpenTabsDelegate;
    assertTrue(!!openTabsDelegate);

    const dummyTabItem = {
      title: ['Open Tab'],
      data: {
        type: OpenTabsItemType.TAB as const,
        tab: {tabId: 1, alertStates: []} as unknown as Tab,
      },
    };
    const dummySplitTabItem = {
      title: ['Split View'],
      data: {
        type: OpenTabsItemType.SPLIT_TAB as const,
        tabs: [
          {tabId: 2, alertStates: []} as unknown as Tab,
          {tabId: 3, alertStates: []} as unknown as Tab,
        ] as [Tab, Tab],
      },
    };

    // Closing a single tab records 1.
    openTabsDelegate.onItemActionButtonClicked(dummyTabItem);
    triggerClose();
    assertEquals(
        1, metrics.count('Tabs.Organizer.NumTabsClosedPerInstance', 1));

    // Closing a split tab records 2.
    openTabsDelegate.onItemActionButtonClicked(dummySplitTabItem);
    triggerClose();
    assertEquals(
        1, metrics.count('Tabs.Organizer.NumTabsClosedPerInstance', 2));

    // Closing multiple tabs and then switching to a tab still records the
    // total number of tabs closed in the session.
    openTabsDelegate.onItemActionButtonClicked(dummyTabItem);
    openTabsDelegate.onItemActionButtonClicked(dummySplitTabItem);
    openTabsDelegate.onItemClick(dummyTabItem);
    triggerClose();
    assertEquals(
        1, metrics.count('Tabs.Organizer.NumTabsClosedPerInstance', 3));

    // Resets to 0 on subsequent close without closing tabs.
    triggerClose();
    assertEquals(
        2, metrics.count('Tabs.Organizer.NumTabsClosedPerInstance', 0));
  });

  test('records StartSearch user action at most once per session', () => {
    // Empty search does not record StartSearch.
    app.$.searchField.setValue('');
    assertEquals(0, metrics.count('OrganizerPanel.StartSearch'));

    // Entering a search query records StartSearch once.
    app.$.searchField.setValue('a');
    assertEquals(1, metrics.count('OrganizerPanel.StartSearch'));

    // Subsequent queries in the same session do not record again, even
    // after clearing the search field.
    app.$.searchField.setValue('ab');
    app.$.searchField.setValue('');
    app.$.searchField.setValue('c');
    assertEquals(1, metrics.count('OrganizerPanel.StartSearch'));

    // Hiding the panel starts a new session, so searching again records
    // StartSearch once more.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    assertEquals(1, metrics.count('OrganizerPanel.StartSearch'));

    app.$.searchField.setValue('d');
    assertEquals(2, metrics.count('OrganizerPanel.StartSearch'));
  });
});
