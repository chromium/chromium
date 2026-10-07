// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import {INITIAL_ITEM_COUNT, organizerPanelBrowserProxyFactory, OrganizerPanelPageHandlerRemote, SearchApiProxyImpl} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionElement, OrganizerListSectionItem, OrganizerListSectionItemElement, OrganizerPanelPageRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchApiProxy} from './test_search_api_proxy.js';
import {TestSectionDelegate} from './test_section_delegate.js';

suite('OrganizerListSectionTest', () => {
  let listSection: OrganizerListSectionElement;
  let mockHandler: TestMock<OrganizerPanelPageHandlerRemote>&
      OrganizerPanelPageHandlerRemote;
  let remotePage: OrganizerPanelPageRemote;
  let testSearchProxy: TestSearchApiProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockHandler = TestMock.fromClass(OrganizerPanelPageHandlerRemote);
    mockHandler.setPromiseResolveFor(
        'getSectionState', {state: {expanded: true, showAll: false}});
    const {instance, remote} =
        organizerPanelBrowserProxyFactory.createForTest(mockHandler);
    organizerPanelBrowserProxyFactory.setInstance(instance);
    remotePage = remote;
    testSearchProxy = new TestSearchApiProxy();
    SearchApiProxyImpl.setInstance(testSearchProxy);
    listSection = document.createElement('organizer-list-section');
    document.body.appendChild(listSection);
    await microtasksFinished();
  });

  test('renders header and items from delegate', async () => {
    const items: Array<OrganizerListSectionItem<unknown>> = [
      {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
      {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
    ];
    listSection.delegate = new TestSectionDelegate('Open Tabs', items);
    await microtasksFinished();

    const header = listSection.$.header;
    assertTrue(!!header);
    assertEquals('Open Tabs', header.textContent.trim());

    const listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, listItems.length);
    assertDeepEquals(['Tab 1'], listItems[0]!.item.title);
    assertDeepEquals(['Tab 2'], listItems[1]!.item.title);
  });

  test(
      'updates items and renders when a remote update is triggered',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
        ];
        listSection.delegate = new TestSectionDelegate('Open Tabs', items);
        await microtasksFinished();

        let listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(1, listItems.length);

        listSection.onItemsChanged([
          {
            title: ['Tab 1 Updated'],
            description: [{text: 'tab1.com'}, {text: 'updated'}],
          },
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
        ]);
        await microtasksFinished();

        listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(2, listItems.length);
        assertDeepEquals(['Tab 1 Updated'], listItems[0]!.item.title);
        assertDeepEquals(
            [{text: 'tab1.com'}, {text: 'updated'}],
            listItems[0]!.$.description.descriptionParts);
        assertDeepEquals(['Tab 2'], listItems[1]!.item.title);
      });

  test('collapses and expands section when header is clicked', async () => {
    const items: Array<OrganizerListSectionItem<unknown>> = [
      {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
      {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
      {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
      {title: ['Tab 4'], description: [{text: 'tab4.com'}]},
    ];
    listSection.delegate =
        new TestSectionDelegate('Open Tabs', items, undefined, 'open-tabs');
    await microtasksFinished();

    const header = listSection.$.header;
    const itemsContainer =
        listSection.shadowRoot.querySelector<CrCollapseElement>('#items')!;

    const renderedItems =
        itemsContainer.querySelectorAll<OrganizerListSectionItemElement>(
            'organizer-list-section-item');
    assertEquals(INITIAL_ITEM_COUNT, renderedItems.length);
    assertTrue(header.expanded);
    assertTrue(itemsContainer.opened);
    assertEquals(0, mockHandler.getCallCount('setSectionExpanded'));

    header.$.expandButton.click();
    await microtasksFinished();

    assertFalse(header.expanded);
    assertFalse(itemsContainer.opened);
    assertEquals(1, mockHandler.getCallCount('setSectionExpanded'));
    assertDeepEquals(
        ['open-tabs', false], mockHandler.getArgs('setSectionExpanded')[0]);

    header.$.expandButton.click();
    await microtasksFinished();

    assertTrue(header.expanded);
    assertTrue(itemsContainer.opened);
    assertEquals(2, mockHandler.getCallCount('setSectionExpanded'));
    assertDeepEquals(
        ['open-tabs', true], mockHandler.getArgs('setSectionExpanded')[1]);
  });

  test(
      'initializes collapsed when getSectionState returns expanded false',
      async () => {
        mockHandler.setPromiseResolveFor(
            'getSectionState', {state: {expanded: false, showAll: false}});
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
        ];
        listSection.delegate =
            new TestSectionDelegate('Open Tabs', items, undefined, 'open-tabs');
        await microtasksFinished();

        const header = listSection.$.header;
        const itemsContainer =
            listSection.shadowRoot.querySelector<CrCollapseElement>('#items')!;
        assertEquals('open-tabs', mockHandler.getArgs('getSectionState')[0]);
        assertFalse(header.expanded);
        assertFalse(itemsContainer.opened);
        assertEquals(0, mockHandler.getCallCount('setSectionExpanded'));
      });

  test(
      'updates expanded state when onSectionsStateChanged is called',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
        ];
        listSection.delegate =
            new TestSectionDelegate('Open Tabs', items, undefined, 'open-tabs');
        await microtasksFinished();

        const header = listSection.$.header;
        const itemsContainer =
            listSection.shadowRoot.querySelector<CrCollapseElement>('#items')!;
        assertTrue(header.expanded);
        assertTrue(itemsContainer.opened);

        remotePage.onSectionsStateChanged(
            {'open-tabs': {expanded: false, showAll: false}});
        await microtasksFinished();

        assertFalse(header.expanded);
        assertFalse(itemsContainer.opened);
        assertEquals(0, mockHandler.getCallCount('setSectionExpanded'));

        remotePage.onSectionsStateChanged(
            {'open-tabs': {expanded: true, showAll: false}});
        await microtasksFinished();

        assertTrue(header.expanded);
        assertTrue(itemsContainer.opened);
        assertEquals(0, mockHandler.getCallCount('setSectionExpanded'));
      });

  test(
      'shows some or all items when header menu options are clicked',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
          {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
          {title: ['Tab 4'], description: [{text: 'tab4.com'}]},
          {title: ['Tab 5'], description: [{text: 'tab5.com'}]},
        ];
        listSection.delegate =
            new TestSectionDelegate('Open Tabs', items, undefined, 'open-tabs');
        await microtasksFinished();

        const header = listSection.$.header;
        let renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, renderedItems.length);
        assertDeepEquals(['Tab 1'], renderedItems[0]!.item.title);
        assertDeepEquals(['Tab 2'], renderedItems[1]!.item.title);
        assertDeepEquals(['Tab 3'], renderedItems[2]!.item.title);
        assertEquals(0, mockHandler.getCallCount('setSectionShowAll'));

        header.$.menuButton.click();
        await microtasksFinished();
        header.$.showAllButton.click();
        await microtasksFinished();

        renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(5, renderedItems.length);
        assertEquals(1, mockHandler.getCallCount('setSectionShowAll'));
        assertDeepEquals(
            ['open-tabs', true], mockHandler.getArgs('setSectionShowAll')[0]);

        header.$.menuButton.click();
        await microtasksFinished();
        header.$.showSomeButton.click();
        await microtasksFinished();

        renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, renderedItems.length);
        assertEquals(2, mockHandler.getCallCount('setSectionShowAll'));
        assertDeepEquals(
            ['open-tabs', false], mockHandler.getArgs('setSectionShowAll')[1]);

        listSection.searchQuery = 'Tab';
        await microtasksFinished();
        renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(5, renderedItems.length);

        listSection.searchQuery = '';
        await microtasksFinished();
        renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, renderedItems.length);
      });

  test(
      'initializes with all items when getSectionState returns showAll true',
      async () => {
        mockHandler.setPromiseResolveFor(
            'getSectionState', {state: {expanded: true, showAll: true}});
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
          {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
          {title: ['Tab 4'], description: [{text: 'tab4.com'}]},
          {title: ['Tab 5'], description: [{text: 'tab5.com'}]},
        ];
        listSection.delegate =
            new TestSectionDelegate('Open Tabs', items, undefined, 'open-tabs');
        await microtasksFinished();

        const renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals('open-tabs', mockHandler.getArgs('getSectionState')[0]);
        assertEquals(5, renderedItems.length);
        assertEquals(0, mockHandler.getCallCount('setSectionShowAll'));
      });

  test(
      'updates showAll state when onSectionsStateChanged is called',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
          {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
          {title: ['Tab 4'], description: [{text: 'tab4.com'}]},
          {title: ['Tab 5'], description: [{text: 'tab5.com'}]},
        ];
        listSection.delegate =
            new TestSectionDelegate('Open Tabs', items, undefined, 'open-tabs');
        await microtasksFinished();

        let renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, renderedItems.length);

        remotePage.onSectionsStateChanged(
            {'open-tabs': {expanded: true, showAll: true}});
        await microtasksFinished();

        renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(5, renderedItems.length);
        assertEquals(0, mockHandler.getCallCount('setSectionShowAll'));

        remotePage.onSectionsStateChanged(
            {'open-tabs': {expanded: true, showAll: false}});
        await microtasksFinished();

        renderedItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, renderedItems.length);
        assertEquals(0, mockHandler.getCallCount('setSectionShowAll'));
      });

  test(
      'notifies delegate and closes panel when an item is clicked',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
        ];
        const delegate = new TestSectionDelegate('Open Tabs', items);
        listSection.delegate = delegate;
        await microtasksFinished();

        const listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(2, listItems.length);

        listItems[1]!.click();
        assertEquals(1, delegate.getClickCount());
        assertEquals(items[1], delegate.getLastClickedItem());
        assertEquals(1, mockHandler.getCallCount('closePanel'));
      });

  test('notifies delegate when an item action button is clicked', async () => {
    const items: Array<OrganizerListSectionItem<unknown>> = [
      {
        title: ['Tab 1'],
        description: [{text: 'tab1.com'}],
        hoveredActionButton: {
          icon: 'cr:close',
          ariaLabel: 'Close tab',
        },
      },
      {
        title: ['Tab 2'],
        description: [{text: 'tab2.com'}],
        hoveredActionButton: {
          icon: 'cr:close',
          ariaLabel: 'Close tab',
        },
      },
    ];
    const delegate = new TestSectionDelegate('Open Tabs', items);
    listSection.delegate = delegate;
    await microtasksFinished();

    const listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, listItems.length);

    const actionButton =
        listItems[1]!.shadowRoot.querySelector<HTMLElement>('#actionButton')!;
    assertTrue(!!actionButton);

    actionButton.click();
    await microtasksFinished();

    assertEquals(1, delegate.getActionButtonClickCount());
    assertEquals(items[1], delegate.getLastActionButtonClickedItem());
    assertEquals(actionButton, delegate.getLastActionButtonElement());
    assertEquals(0, delegate.getClickCount());
  });

  test('notifies delegate when an item is right-clicked', async () => {
    const items: Array<OrganizerListSectionItem<unknown>> = [
      {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
      {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
    ];
    const delegate = new TestSectionDelegate('Open Tabs', items);
    listSection.delegate = delegate;
    await microtasksFinished();

    const listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, listItems.length);

    listItems[1]!.$.crUrlListItem.dispatchEvent(new MouseEvent('contextmenu', {
      bubbles: true,
      cancelable: true,
      clientX: 42,
      clientY: 84,
    }));
    await microtasksFinished();

    assertEquals(1, delegate.getContextMenuClickCount());
    assertEquals(items[1], delegate.getLastContextMenuClickedItem());
    assertDeepEquals({x: 42, y: 84}, delegate.getLastContextMenuCoordinates());
    assertEquals(0, delegate.getClickCount());
    assertEquals(0, mockHandler.getCallCount('closePanel'));
  });

  test('filters items based on searchQuery', async () => {
    const delegateItems: Array<OrganizerListSectionItem<unknown>> = [
      {title: ['Google'], description: [{text: 'google.com'}]},
      {title: ['YouTube'], description: [{text: 'youtube.com'}]},
    ];
    listSection.delegate = new TestSectionDelegate('Open Tabs', delegateItems);
    await microtasksFinished();

    let listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, listItems.length);
    assertEquals(null, listSection.shadowRoot.querySelector('#noResults'));

    async function setSearchQuery(query: string) {
      listSection.searchQuery = query;
      await microtasksFinished();
    }

    await setSearchQuery('You');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(1, listItems.length);
    assertDeepEquals(['YouTube'], listItems[0]!.item.title);
    assertEquals(null, listSection.shadowRoot.querySelector('#noResults'));

    await setSearchQuery('google.com');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(1, listItems.length);
    assertDeepEquals(['Google'], listItems[0]!.item.title);
    assertEquals(null, listSection.shadowRoot.querySelector('#noResults'));

    await setSearchQuery('nomatch');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(0, listItems.length);
    const noResults = listSection.shadowRoot.querySelector('#noResults');
    assertTrue(!!noResults);
    assertEquals('No results', noResults.textContent.trim());

    await setSearchQuery('');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, listItems.length);
    assertEquals(null, listSection.shadowRoot.querySelector('#noResults'));
  });

  test(
      'shows no results message only when search query has no matches',
      async () => {
        listSection.delegate = new TestSectionDelegate('Open Tabs', []);
        await microtasksFinished();

        assertEquals(null, listSection.shadowRoot.querySelector('#noResults'));

        listSection.searchQuery = 'query';
        await microtasksFinished();

        const noResults = listSection.shadowRoot.querySelector('#noResults');
        assertTrue(!!noResults);
        assertEquals('No results', noResults.textContent.trim());
      });

  test(
      'force-expands section and disables header toggle when searching',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
          {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
          {title: ['Tab 4'], description: [{text: 'tab4.com'}]},
        ];
        listSection.delegate = new TestSectionDelegate('Open Tabs', items);
        await microtasksFinished();

        const header = listSection.$.header;
        const itemsContainer =
            listSection.shadowRoot.querySelector<CrCollapseElement>('#items')!;

        header.$.expandButton.click();
        await microtasksFinished();

        assertFalse(header.expanded);
        assertFalse(header.disabled);
        assertFalse(itemsContainer.opened);

        listSection.searchQuery = 'Tab';
        await microtasksFinished();

        assertTrue(header.expanded);
        assertTrue(header.disabled);
        assertTrue(itemsContainer.opened);
        const listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(4, listItems.length);

        listSection.searchQuery = '';
        await microtasksFinished();

        assertFalse(header.expanded);
        assertFalse(header.disabled);
        assertFalse(itemsContainer.opened);
      });

  test('highlights matching text when searching', async () => {
    const delegateItems = [
      {
        title: ['Google Search'],
        description: [{text: 'google.com'}],
      },
      {
        title: ['YouTube Music'],
        description: [{text: 'music.youtube.com'}],
      },
    ];
    listSection.delegate = new TestSectionDelegate('Open Tabs', delegateItems);
    await microtasksFinished();

    listSection.searchQuery = 'Music';
    await microtasksFinished();

    const listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(1, listItems.length);
    assertDeepEquals(
        [[{start: 8, length: 5}]], listItems[0]!.$.title.highlightRanges);

    assertDeepEquals(
        [[{start: 0, length: 5}]], listItems[0]!.$.description.highlightRanges);

    listSection.searchQuery = '';
    await microtasksFinished();

    const clearedItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, clearedItems.length);
    assertDeepEquals([[]], clearedItems[0]!.$.title.highlightRanges);
    assertDeepEquals([[]], clearedItems[1]!.$.title.highlightRanges);
    assertDeepEquals([[]], clearedItems[0]!.$.description.highlightRanges);
    assertDeepEquals([[]], clearedItems[1]!.$.description.highlightRanges);
  });

  test('renders zero state from delegate when there are no items', async () => {
    const zeroState = html`<div id="zeroState">Create tab group</div>`;
    listSection.delegate = new TestSectionDelegate('Tab Groups', [], zeroState);
    await microtasksFinished();

    const itemsContainer =
        listSection.shadowRoot.querySelector<CrCollapseElement>('#items');
    assertTrue(!!itemsContainer);
    assertTrue(itemsContainer.opened);
    const zeroStateElement = itemsContainer.querySelector('#zeroState');
    assertTrue(!!zeroStateElement);
    assertEquals('Create tab group', zeroStateElement.textContent.trim());
    assertEquals(
        0,
        itemsContainer.querySelectorAll('organizer-list-section-item').length);
    assertEquals(null, listSection.shadowRoot.querySelector('#noResults'));
  });

  test(
      'does not render zero state when items exist or when searching',
      async () => {
        const zeroState = html`<div id="zeroState">Create tab group</div>`;
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab Group 1']},
        ];
        listSection.delegate =
            new TestSectionDelegate('Tab Groups', items, zeroState);
        await microtasksFinished();

        assertEquals(null, listSection.shadowRoot.querySelector('#zeroState'));
        assertEquals(
            1,
            listSection.shadowRoot
                .querySelectorAll('organizer-list-section-item')
                .length);

        // Searching with no matches should show #noResults, not zero state.
        listSection.searchQuery = 'nomatch';
        await microtasksFinished();

        assertEquals(null, listSection.shadowRoot.querySelector('#zeroState'));
        const noResults = listSection.shadowRoot.querySelector('#noResults');
        assertTrue(!!noResults);
        assertEquals('No results', noResults.textContent.trim());
      });

  test(
      'renders zero state when items are cleared on a delegate with zero state',
      async () => {
        const zeroState = html`<div id="zeroState">Create tab group</div>`;
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab Group 1']},
        ];
        listSection.delegate =
            new TestSectionDelegate('Tab Groups', items, zeroState);
        await microtasksFinished();

        assertEquals(null, listSection.shadowRoot.querySelector('#zeroState'));

        listSection.onItemsChanged([]);
        await microtasksFinished();

        const zeroStateElement =
            listSection.shadowRoot.querySelector('#zeroState');
        assertTrue(!!zeroStateElement);
        assertEquals('Create tab group', zeroStateElement.textContent.trim());
      });

  test(
      'always renders zero state when shouldAlwaysShowZeroState is true',
      async () => {
        const zeroState = html`<div id="zeroState">Create tab group</div>`;
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab Group 1']},
          {title: ['Tab Group 2']},
          {title: ['Tab Group 3']},
          {title: ['Tab Group 4']},
        ];
        listSection.delegate = new TestSectionDelegate(
            'Tab Groups', items, zeroState, 'tab-groups', true);
        await microtasksFinished();

        const itemsContainer =
            listSection.shadowRoot.querySelector<CrCollapseElement>('#items')!;
        assertTrue(!!itemsContainer);
        assertTrue(itemsContainer.opened);

        const zeroStateElement = itemsContainer.querySelector('#zeroState');
        assertTrue(!!zeroStateElement);
        assertEquals('Create tab group', zeroStateElement.textContent.trim());
        assertEquals(zeroStateElement, itemsContainer.firstElementChild);

        const renderedItems =
            itemsContainer.querySelectorAll('organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, renderedItems.length);

        // Searching hides zero state even when shouldAlwaysShowZeroState is
        // true.
        listSection.searchQuery = 'Tab Group 1';
        await microtasksFinished();

        assertEquals(null, listSection.shadowRoot.querySelector('#zeroState'));
        assertEquals(
            1,
            listSection.shadowRoot
                .querySelectorAll('organizer-list-section-item')
                .length);

        // Clearing search restores zero state at the top.
        listSection.searchQuery = '';
        await microtasksFinished();

        assertTrue(!!listSection.shadowRoot.querySelector('#zeroState'));
        assertEquals(
            INITIAL_ITEM_COUNT,
            listSection.shadowRoot
                .querySelectorAll('organizer-list-section-item')
                .length);
      });
});
