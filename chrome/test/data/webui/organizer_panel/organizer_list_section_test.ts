// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import {INITIAL_ITEM_COUNT, SearchApiProxyImpl} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionElement, OrganizerListSectionItem, OrganizerListSectionItemElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchApiProxy} from './test_search_api_proxy.js';
import {TestSectionDelegate} from './test_section_delegate.js';

suite('OrganizerListSectionTest', () => {
  let listSection: OrganizerListSectionElement;
  let testSearchProxy: TestSearchApiProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    assertEquals('Open Tabs', header.textContent);

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

  test(
      'renders initial items and expand button when items exceed initial count',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
          {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
          {title: ['Tab 4'], description: [{text: 'tab4.com'}]},
        ];
        listSection.delegate = new TestSectionDelegate('Open Tabs', items);
        await microtasksFinished();

        const itemsContainer = listSection.shadowRoot.querySelector('#items')!;
        const collapse =
            listSection.shadowRoot.querySelector<HTMLElement>('#collapse');
        assertTrue(!!collapse);
        const expandButton =
            listSection.shadowRoot.querySelector<CrUrlListItemElement>(
                '#expandButton');
        assertTrue(!!expandButton);
        const expandButtonIcon =
            expandButton.querySelector<CrIconElement>('#expandButtonIcon');
        assertTrue(!!expandButtonIcon);

        const initialItems =
            itemsContainer.querySelectorAll<OrganizerListSectionItemElement>(
                ':scope > organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, initialItems.length);
        assertDeepEquals(['Tab 1'], initialItems[0]!.item.title);
        assertDeepEquals(['Tab 2'], initialItems[1]!.item.title);
        assertDeepEquals(['Tab 3'], initialItems[2]!.item.title);

        assertEquals(
            0, collapse.querySelectorAll('organizer-list-section-item').length);

        const children = Array.from(itemsContainer.children);
        assertEquals(5, children.length);
        assertEquals(initialItems[0], children[0]);
        assertEquals(initialItems[1], children[1]);
        assertEquals(initialItems[2], children[2]);
        assertEquals(collapse, children[3]);
        assertEquals(expandButton, children[4]);

        assertEquals('compact', expandButton.size);

        const expandButtonIconContainer =
            expandButton.shadowRoot.querySelector<HTMLElement>(
                '#iconContainer');
        assertTrue(!!expandButtonIconContainer);
        const itemIconContainer =
            initialItems[0]!.$.crUrlListItem.shadowRoot
                .querySelector<HTMLElement>('#iconContainer');
        assertTrue(!!itemIconContainer);
        assertEquals('40px', getComputedStyle(itemIconContainer).width);
        assertEquals('40px', getComputedStyle(expandButtonIconContainer).width);

        const focusable = expandButton.getFocusableElement();
        assertFalse(listSection.hasAttribute('expanded_'));
        assertEquals('Show more', expandButton.title);
        assertEquals('cr:keyboard-arrow-down', expandButtonIcon.icon);
        assertEquals('false', focusable.getAttribute('aria-expanded'));

        focusable.click();
        await microtasksFinished();

        assertTrue(listSection.hasAttribute('expanded_'));
        assertEquals('Show less', expandButton.title);
        assertEquals('cr:keyboard-arrow-up', expandButtonIcon.icon);
        assertEquals('true', focusable.getAttribute('aria-expanded'));

        const collapsedItems =
            collapse.querySelectorAll('organizer-list-section-item');
        assertEquals(1, collapsedItems.length);
        assertDeepEquals(['Tab 4'], collapsedItems[0]!.item.title);
        assertEquals(expandButton, itemsContainer.lastElementChild);

        focusable.click();
        await microtasksFinished();

        assertFalse(listSection.hasAttribute('expanded_'));
        assertEquals('Show more', expandButton.title);
        assertEquals('cr:keyboard-arrow-down', expandButtonIcon.icon);
        assertEquals('false', focusable.getAttribute('aria-expanded'));
        assertEquals(
            1, collapse.querySelectorAll('organizer-list-section-item').length);

        collapse.dispatchEvent(
            new TransitionEvent('transitionend', {propertyName: 'height'}));
        await microtasksFinished();

        assertEquals(
            0, collapse.querySelectorAll('organizer-list-section-item').length);
        assertEquals(expandButton, itemsContainer.lastElementChild);
      });

  test(
      'does not render expand button when items do not exceed initial count',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
          {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
        ];
        listSection.delegate = new TestSectionDelegate('Open Tabs', items);
        await microtasksFinished();

        const expandButton =
            listSection.shadowRoot.querySelector('#expandButton');
        assertEquals(null, expandButton);
      });

  test('notifies delegate when an item is clicked', async () => {
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

    listItems[1]!.click();
    assertEquals(1, delegate.getClickCount());
    assertEquals(items[1], delegate.getLastClickedItem());
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
      'shows all matching items without expand button when searching',
      async () => {
        const items: Array<OrganizerListSectionItem<unknown>> = [
          {title: ['Tab 1'], description: [{text: 'tab1.com'}]},
          {title: ['Tab 2'], description: [{text: 'tab2.com'}]},
          {title: ['Tab 3'], description: [{text: 'tab3.com'}]},
          {title: ['Tab 4'], description: [{text: 'tab4.com'}]},
        ];
        listSection.delegate = new TestSectionDelegate('Open Tabs', items);
        await microtasksFinished();

        let listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, listItems.length);
        assertTrue(!!listSection.shadowRoot.querySelector('#expandButton'));

        listSection.searchQuery = 'Tab';
        await microtasksFinished();

        listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(4, listItems.length);
        assertEquals(
            null, listSection.shadowRoot.querySelector('#expandButton'));

        listSection.searchQuery = '';
        await microtasksFinished();

        listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, listItems.length);
        assertTrue(!!listSection.shadowRoot.querySelector('#expandButton'));
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
