// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionItemElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerListSectionItemTest', () => {
  let listItem: OrganizerListSectionItemElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    listItem = document.createElement('organizer-list-section-item');
    document.body.appendChild(listItem);
    await microtasksFinished();
  });

  test('renders title and description', async () => {
    listItem.item = {
      title: 'Google Search',
      description: ['google.com', '5 mins ago'],
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertEquals('Google Search', crUrlListItem.title);
    assertEquals('google.com · 5 mins ago', crUrlListItem.description);
  });

  test('renders prefix icon with URL', async () => {
    listItem.item = {
      title: 'Google Search',
      prefixIcon: {
        urls: ['https://google.com'],
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertEquals('https://google.com', crUrlListItem.url);
  });

  test('renders prefix icon with custom element', async () => {
    listItem.item = {
      title: 'Tab Group',
      prefixIcon: {
        element: html`<span id="customGroupIcon">Group</span>`,
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    const customIcon = crUrlListItem.querySelector('#customGroupIcon');
    assertTrue(!!customIcon);
    assertEquals('Group', customIcon.textContent);
  });

  test('renders trailing icon only', async () => {
    listItem.item = {
      title: 'Starred Tab',
      trailingIcon: 'cr:star',
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertTrue(crUrlListItem.hasAttribute('always-show-suffix'));

    const trailingIcon = listItem.$.trailingIcon;
    assertTrue(!!trailingIcon);
    assertEquals('cr:star', trailingIcon.icon);
    assertFalse(trailingIcon.classList.contains('has-action-button'));
    const actionButton = crUrlListItem.querySelector('#actionButton');
    assertEquals(null, actionButton);

    assertTrue(isVisible(trailingIcon));

    // Trailing icon should remain visible when hovered.
    listItem.classList.add('hovered');
    assertTrue(isVisible(trailingIcon));
  });

  test('renders hovered action button only', async () => {
    listItem.item = {
      title: 'Tab',
      hoveredActionButton: {
        icon: 'cr:close',
        ariaLabel: 'Close tab',
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertTrue(crUrlListItem.hasAttribute('always-show-suffix'));

    const trailingIcon = crUrlListItem.querySelector('#trailingIcon');
    assertEquals(null, trailingIcon);

    const actionButton = listItem.$.actionButton;
    assertTrue(!!actionButton);
    assertEquals('cr:close', actionButton.getAttribute('iron-icon'));
    assertEquals('Close tab', actionButton.getAttribute('aria-label'));

    // Action button should be hidden when not hovered.
    assertFalse(isVisible(actionButton));

    // Action button should be displayed when hovered.
    listItem.classList.add('hovered');
    assertTrue(isVisible(actionButton));
  });

  test('switches from trailing icon to action button on hover', async () => {
    listItem.item = {
      title: 'Pinned Tab Group',
      trailingIcon: 'cr:star',
      hoveredActionButton: {
        icon: 'cr:star-border',
        ariaLabel: 'Unpin group',
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);

    const trailingIcon = listItem.$.trailingIcon;
    assertTrue(!!trailingIcon);
    assertTrue(trailingIcon.classList.contains('has-action-button'));

    const actionButton = listItem.$.actionButton;
    assertTrue(!!actionButton);

    // Initially, trailing icon is visible and action button is hidden.
    assertTrue(isVisible(trailingIcon));
    assertFalse(isVisible(actionButton));

    // When hovered, action button becomes visible and trailing icon is hidden.
    listItem.classList.add('hovered');
    assertFalse(isVisible(trailingIcon));
    assertTrue(isVisible(actionButton));

    // When unhovered, trailing icon becomes visible again and action button is
    // hidden.
    listItem.classList.remove('hovered');
    assertTrue(isVisible(trailingIcon));
    assertFalse(isVisible(actionButton));
  });

  test(
      'clicking action button dispatches event and stops propagation',
      async () => {
        const item = {
          title: 'Closeable Tab',
          hoveredActionButton: {
            icon: 'cr:close',
            ariaLabel: 'Close tab',
          },
        };
        listItem.item = item;
        await microtasksFinished();

        const actionButton = listItem.$.actionButton;
        assertTrue(!!actionButton);

        let itemClicked = false;
        listItem.addEventListener('click', () => {
          itemClicked = true;
        });

        const actionClickPromise =
            eventToPromise('action-button-click', listItem);
        actionButton.click();
        const actionEvent = await actionClickPromise as CustomEvent<{
                              item: typeof item,
                              buttonElement: HTMLElement,
                            }>;

        assertEquals(item, actionEvent.detail.item);
        assertEquals(actionButton, actionEvent.detail.buttonElement);
        assertFalse(itemClicked);
      });
});
