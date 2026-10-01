// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import {organizerPanelBrowserProxyFactory, OrganizerPanelPageHandlerRemote, SearchApiProxyImpl} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionElement, OrganizerListSectionItem, OrganizerListSectionItemElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchApiProxy} from './test_search_api_proxy.js';
import {TestSectionDelegate} from './test_section_delegate.js';

suite('OrganizerListSectionTest', () => {
  let listSection: OrganizerListSectionElement;
  let mockHandler: TestMock<OrganizerPanelPageHandlerRemote>&
      OrganizerPanelPageHandlerRemote;
  let testSearchProxy: TestSearchApiProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockHandler = TestMock.fromClass(OrganizerPanelPageHandlerRemote);
    organizerPanelBrowserProxyFactory.setInstance({handler: mockHandler});
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
    listSection.delegate = new TestSectionDelegate('Open Tabs', items);
    await microtasksFinished();

    const header = listSection.$.header;
    const headerIcon = header.$.icon;
    const itemsContainer =
        listSection.shadowRoot.querySelector<CrCollapseElement>('#items')!;

    const renderedItems =
        itemsContainer.querySelectorAll<OrganizerListSectionItemElement>(
            'organizer-list-section-item');
    assertEquals(4, renderedItems.length);
    assertTrue(header.expanded);
    assertTrue(itemsContainer.opened);
    assertEquals('cr:keyboard-arrow-up', headerIcon.ironIcon);
    assertEquals('true', headerIcon.getAttribute('aria-expanded'));

    header.click();
    await microtasksFinished();

    assertFalse(header.expanded);
    assertFalse(itemsContainer.opened);
    assertEquals('cr:keyboard-arrow-down', headerIcon.ironIcon);
    assertEquals('false', headerIcon.getAttribute('aria-expanded'));

    header.click();
    await microtasksFinished();

    assertTrue(header.expanded);
    assertTrue(itemsContainer.opened);
    assertEquals('cr:keyboard-arrow-up', headerIcon.ironIcon);
    assertEquals('true', headerIcon.getAttribute('aria-expanded'));
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

    const actionButton = listItems[1]!.$.actionButton;
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
        const headerIcon = header.$.icon;
        const itemsContainer =
            listSection.shadowRoot.querySelector<CrCollapseElement>('#items')!;

        header.click();
        await microtasksFinished();

        assertFalse(header.expanded);
        assertFalse(header.disabled);
        assertFalse(itemsContainer.opened);
        assertEquals('flex', getComputedStyle(headerIcon).display);

        listSection.searchQuery = 'Tab';
        await microtasksFinished();

        assertTrue(header.expanded);
        assertTrue(header.disabled);
        assertTrue(itemsContainer.opened);
        assertEquals('none', getComputedStyle(headerIcon).display);
        const listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(4, listItems.length);

        listSection.searchQuery = '';
        await microtasksFinished();

        assertFalse(header.expanded);
        assertFalse(header.disabled);
        assertFalse(itemsContainer.opened);
        assertEquals('flex', getComputedStyle(headerIcon).display);
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
});
