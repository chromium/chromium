// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/organizer_panel.js';

import type {OrganizerListSectionItemElement, StackedFaviconsElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

const TEST_TITLE = 'Google Search';
const TEST_DESCRIPTION = ['google.com', '5 mins ago'];
const EXPECTED_DESCRIPTION = 'google.com · 5 mins ago';
const TEST_URL_1 = 'https://google.com';
const TEST_URL_2 = 'https://youtube.com';

const TEST_FAVICON_1 = getFaviconForPageURL(TEST_URL_1, false);
const TEST_FAVICON_2 = getFaviconForPageURL(TEST_URL_2, false);

const TEST_CUSTOM_ICON_ID = 'customGroupIcon';
const TEST_CUSTOM_ICON_TEXT = 'Group';

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
      title: TEST_TITLE,
      description: TEST_DESCRIPTION,
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertEquals(TEST_TITLE, crUrlListItem.title);
    assertEquals(EXPECTED_DESCRIPTION, crUrlListItem.description);
  });

  test('renders prefix icon with URL', async () => {
    listItem.item = {
      title: TEST_TITLE,
      prefixIcon: {
        url: TEST_URL_1,
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    assertEquals(TEST_URL_1, crUrlListItem.url);
  });

  test('renders prefix icon with custom element', async () => {
    listItem.item = {
      title: 'Tab Group',
      prefixIcon: {
        element: html`<span id="${TEST_CUSTOM_ICON_ID}">${
            TEST_CUSTOM_ICON_TEXT}</span>`,
      },
    };
    await microtasksFinished();

    const crUrlListItem = listItem.$.crUrlListItem;
    assertTrue(!!crUrlListItem);
    const customIcon = crUrlListItem.querySelector(`#${TEST_CUSTOM_ICON_ID}`);
    assertTrue(!!customIcon);
    assertEquals(TEST_CUSTOM_ICON_TEXT, customIcon.textContent);
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

  test(
      'renders prefix icon with multiple URLs as stacked favicons',
      async () => {
        listItem.item = {
          title: 'Split View',
          prefixIcon: {
            stackedFavicons: {
              urls: [TEST_URL_1, TEST_URL_2],
              stackVertically: false,
            },
          },
        };
        await microtasksFinished();

        const crUrlListItem = listItem.$.crUrlListItem;
        assertTrue(!!crUrlListItem);
        assertEquals(undefined, crUrlListItem.url);

        const stackedFavicons =
            listItem.shadowRoot.querySelector<StackedFaviconsElement>(
                'stacked-favicons');
        assertTrue(!!stackedFavicons);
        assertEquals('customIcon', stackedFavicons.getAttribute('slot'));
        assertEquals(TEST_URL_1, stackedFavicons.url);
        assertEquals(TEST_URL_2, stackedFavicons.secondaryUrl);
        assertEquals(
            TEST_FAVICON_1,
            stackedFavicons.$.firstFavicon.style.backgroundImage);
        assertEquals(
            TEST_FAVICON_2,
            stackedFavicons.$.secondFavicon.style.backgroundImage);
        assertFalse(stackedFavicons.stackVertically);
      });

  test('forwards stacking orientation to stacked favicons', async () => {
    listItem.item = {
      title: 'Split View Vertical',
      prefixIcon: {
        stackedFavicons: {
          urls: [TEST_URL_1, TEST_URL_2],
          stackVertically: true,
        },
      },
    };
    await microtasksFinished();

    const stackedFavicons =
        listItem.shadowRoot.querySelector<StackedFaviconsElement>(
            'stacked-favicons');
    assertTrue(!!stackedFavicons);
    assertTrue(stackedFavicons.stackVertically);
    assertTrue(stackedFavicons.hasAttribute('stack-vertically'));
  });

  test(
      'does not render stacked favicons when single URL is provided',
      async () => {
        listItem.item = {
          title: 'Single URL',
          prefixIcon: {
            url: TEST_URL_1,
          },
        };
        await microtasksFinished();

        const stackedFavicons =
            listItem.shadowRoot.querySelector('stacked-favicons');
        assertEquals(null, stackedFavicons);
        assertEquals(TEST_URL_1, listItem.$.crUrlListItem.url);
      });
});
