// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import {INITIAL_ITEM_COUNT, SearchApiProxyImpl} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionElement, OrganizerListSectionItem} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
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

        // Initially only the first 3 items should be rendered.
        let listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, listItems.length);
        assertDeepEquals(['Tab 1'], listItems[0]!.item.title);
        assertDeepEquals(['Tab 2'], listItems[1]!.item.title);
        assertDeepEquals(['Tab 3'], listItems[2]!.item.title);

        // Below those items, cr-expand-button with text "Show more".
        const expandButton =
            listSection.shadowRoot.querySelector<CrExpandButtonElement>(
                'cr-expand-button');
        assertTrue(!!expandButton);
        assertEquals('Show more', expandButton.textContent.trim());
        assertFalse(expandButton.expanded);

        // Verify DOM order: the expand button is positioned after the initial
        // items.
        const itemsContainer = listSection.shadowRoot.querySelector('#items')!;
        const children = Array.from(itemsContainer.children);
        assertEquals(listItems[0], children[0]);
        assertEquals(listItems[1], children[1]);
        assertEquals(listItems[2], children[2]);
        assertEquals(expandButton, children[3]);

        // Click the expand button to reveal the rest of the items.
        expandButton.click();
        await microtasksFinished();

        assertTrue(expandButton.expanded);
        listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(4, listItems.length);
        assertDeepEquals(['Tab 4'], listItems[3]!.item.title);

        // Verify the rest of the items are rendered below the expand button.
        const updatedChildren = Array.from(itemsContainer.children);
        assertEquals(listItems[0], updatedChildren[0]);
        assertEquals(listItems[1], updatedChildren[1]);
        assertEquals(listItems[2], updatedChildren[2]);
        assertEquals(expandButton, updatedChildren[3]);
        assertEquals(listItems[3], updatedChildren[4]);

        // Clicking again should collapse the remaining items.
        expandButton.click();
        await microtasksFinished();

        assertFalse(expandButton.expanded);
        listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(INITIAL_ITEM_COUNT, listItems.length);
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
            listSection.shadowRoot.querySelector('cr-expand-button');
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

    async function setSearchQuery(query: string) {
      listSection.searchQuery = query;
      await microtasksFinished();
    }

    await setSearchQuery('You');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(1, listItems.length);
    assertDeepEquals(['YouTube'], listItems[0]!.item.title);

    await setSearchQuery('google.com');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(1, listItems.length);
    assertDeepEquals(['Google'], listItems[0]!.item.title);

    await setSearchQuery('nomatch');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(0, listItems.length);

    await setSearchQuery('');
    listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, listItems.length);
  });
});
