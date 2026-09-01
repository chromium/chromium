// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionElement, OrganizerListSectionItem} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSectionDelegate} from './test_section_delegate.js';

suite('OrganizerListSectionTest', () => {
  let listSection: OrganizerListSectionElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    listSection = document.createElement('organizer-list-section');
    document.body.appendChild(listSection);
    await microtasksFinished();
  });

  test('renders header and items from delegate', async () => {
    const items: OrganizerListSectionItem[] = [
      {title: 'Tab 1', description: ['tab1.com']},
      {title: 'Tab 2', description: ['tab2.com']},
    ];
    listSection.delegate = new TestSectionDelegate('Open Tabs', items);
    await microtasksFinished();

    const header = listSection.$.header;
    assertTrue(!!header);
    assertEquals('Open Tabs', header.textContent);

    const listItems =
        listSection.shadowRoot.querySelectorAll('organizer-list-section-item');
    assertEquals(2, listItems.length);
    assertEquals('Tab 1', listItems[0]!.item.title);
    assertEquals('Tab 2', listItems[1]!.item.title);
  });

  test(
      'updates items and renders when a remote update is triggered',
      async () => {
        const items: OrganizerListSectionItem[] = [
          {title: 'Tab 1', description: ['tab1.com']},
        ];
        listSection.delegate = new TestSectionDelegate('Open Tabs', items);
        await microtasksFinished();

        let listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(1, listItems.length);

        listSection.onItemsChanged([
          {title: 'Tab 1 Updated', description: ['tab1.com', 'updated']},
          {title: 'Tab 2', description: ['tab2.com']},
        ]);
        await microtasksFinished();

        listItems = listSection.shadowRoot.querySelectorAll(
            'organizer-list-section-item');
        assertEquals(2, listItems.length);
        assertEquals('Tab 1 Updated', listItems[0]!.item.title);
        assertEquals(
            'tab1.com · updated', listItems[0]!.$.crUrlListItem.description);
        assertEquals('Tab 2', listItems[1]!.item.title);
      });
});
