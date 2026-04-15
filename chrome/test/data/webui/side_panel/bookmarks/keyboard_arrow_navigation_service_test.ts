// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyArrowNavigationService} from 'chrome://bookmarks-side-panel.top-chrome/keyboard_arrow_navigation_service.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertArrayEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('KeyArrowNavigationServiceTest', () => {
  let service: KeyArrowNavigationService;
  let rootElement: HTMLElement;

  const parentFolder: chrome.bookmarks.BookmarkTreeNode = {
    id: '2',
    parentId: 'SIDE_PANEL_ROOT_BOOKMARK_ID',
    title: 'Other Bookmarks',
    children: [
      {
        id: '3',
        parentId: '2',
        title: 'First child bookmark',
        url: 'http://child/bookmark/1/',
        dateAdded: 1,
        dateLastUsed: 4,
      },
      {
        id: '4',
        parentId: '2',
        title: 'Second child bookmark',
        url: 'http://child/bookmark/2/',
        dateAdded: 3,
        dateLastUsed: 3,
      },
      {
        id: '5',
        parentId: '2',
        title: 'Child folder',
        dateAdded: 2,
        children: [
          {
            id: '6',
            parentId: '5',
            title: 'Nested bookmark',
            url: 'http://nested/bookmark/',
            dateAdded: 4,
          },
        ],
      },
    ],
  };

  function createParentHTMLNode(parentNode: chrome.bookmarks.BookmarkTreeNode):
      HTMLElement {
    const parentElement = document.createElement('div');
    parentElement.id = 'root';
    parentElement.attachShadow({mode: 'open'});

    if (parentNode.children) {
      for (const node of parentNode.children) {
        parentElement.shadowRoot!.append(createChildNodes(node));
      }
    }

    return parentElement;
  }

  function createChildNodes(node: chrome.bookmarks.BookmarkTreeNode):
      HTMLElement {
    const element = document.createElement('div');
    element.id = `box-${node.id}`;
    element.innerText = node.title;
    element.tabIndex = 0;
    element.attachShadow({mode: 'open'});

    const span = document.createElement('span');
    span.innerText = node.title;
    element.appendChild(span);

    if (node.children) {
      for (const child of node.children) {
        element.shadowRoot!.append(createChildNodes(child));
      }
    }

    return element;
  }

  function dispatchArrowEvent(element: HTMLElement, direction: string) {
    element.dispatchEvent(new KeyboardEvent('keydown', {key: direction}));
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    rootElement = createParentHTMLNode(parentFolder);
    document.body.append(rootElement);

    service = new KeyArrowNavigationService(rootElement, 'div');
    service.startListening();
    service.rebuildNavigationElements(rootElement);
    rootElement.focus();
  });

  test('Initializes', () => {
    const elementListIds =
        service.getElementsForTesting().map((el: HTMLElement) => el.id);

    assertArrayEquals(['box-3', 'box-4', 'box-5', 'box-6'], elementListIds);
  });

  test('MovesFocus', () => {
    const elements = service.getElementsForTesting();
    elements[0]!.focus();

    let activeElement = getDeepActiveElement();
    assertEquals(activeElement!.id, 'box-3');

    dispatchArrowEvent(rootElement, 'ArrowDown');

    activeElement = getDeepActiveElement();
    assertEquals(activeElement!.id, 'box-4');

    dispatchArrowEvent(rootElement, 'ArrowDown');

    activeElement = getDeepActiveElement();
    assertEquals(activeElement!.id, 'box-5');

    dispatchArrowEvent(rootElement, 'ArrowUp');
    dispatchArrowEvent(rootElement, 'ArrowUp');

    activeElement = getDeepActiveElement();
    assertEquals(activeElement!.id, 'box-3');
  });

  test('RemovesNestedElement', () => {
    const elements = service.getElementsForTesting();
    elements[0]!.focus();

    let activeElement = getDeepActiveElement();
    assertEquals(activeElement!.id, 'box-3');

    let elementListIds =
        service.getElementsForTesting().map((el: HTMLElement) => el.id);
    assertArrayEquals(['box-3', 'box-4', 'box-5', 'box-6'], elementListIds);

    const targetElement = rootElement.shadowRoot!.querySelector('#box-5')!;
    service.removeElementsWithin(targetElement as HTMLElement);

    elementListIds =
        service.getElementsForTesting().map((el: HTMLElement) => el.id);
    assertArrayEquals(['box-3', 'box-4', 'box-5'], elementListIds);

    dispatchArrowEvent(rootElement, 'ArrowDown');
    dispatchArrowEvent(rootElement, 'ArrowDown');
    dispatchArrowEvent(rootElement, 'ArrowDown');

    activeElement = getDeepActiveElement();
    assertEquals(activeElement!.id, 'box-3');
  });

  test('AddsElements', () => {
    service.removeElementsWithin(rootElement);
    let elementListIds =
        service.getElementsForTesting().map((el: HTMLElement) => el.id);

    assertArrayEquals([], elementListIds);

    service.addElementsWithin(rootElement);

    elementListIds =
        service.getElementsForTesting().map((el: HTMLElement) => el.id);

    assertArrayEquals(['box-3', 'box-4', 'box-5', 'box-6'], elementListIds);
  });

  test('DoesNotCrashOnEmptyList', () => {
    rootElement.shadowRoot?.replaceChildren();
    service.rebuildNavigationElements(rootElement);
    assertEquals(service.getElementCount(), 0);

    // This should not crash
    dispatchArrowEvent(rootElement, 'ArrowDown');
    dispatchArrowEvent(rootElement, 'ArrowUp');
  });

  test('DoesNotCrashOnStaleIndex', () => {
    // Start with 4 elements. Focus the last one (index 3).
    service.moveFocus(1);
    service.moveFocus(1);
    service.moveFocus(1);
    assertEquals(service.getElementAtFocusIndexForTesting().id, 'box-6');

    // Remove box-5 children (box-6). Now elements list has length 3.
    const box5 = rootElement.shadowRoot!.querySelector('#box-5')!;
    service.removeElementsWithin(box5 as HTMLElement);
    assertEquals(service.getElementCount(), 3);

    // This should not crash even if the index was stale
    dispatchArrowEvent(rootElement, 'ArrowDown');
    // The focus index wraps around to 0 after the last element is deleted and
    // then is incremented to point at the second element.
    assertEquals(service.getElementAtFocusIndexForTesting().id, 'box-4');
  });
});
