// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeyArrowNavigationService} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('KeyArrowNavigationServiceTest', () => {
  let service: KeyArrowNavigationService;
  let rootElement: HTMLElement;
  let items: HTMLElement[];

  function createTestDOM(): {root: HTMLElement, items: HTMLElement[]} {
    const root = document.createElement('div');
    root.id = 'root';

    const item1 = document.createElement('div');
    item1.id = 'item1';
    item1.tabIndex = 0;
    item1.attachShadow({mode: 'open'});
    const menuBtn = document.createElement('button');
    menuBtn.id = 'menuButton';
    item1.shadowRoot!.appendChild(menuBtn);

    const item2 = document.createElement('div');
    item2.id = 'item2';
    item2.tabIndex = 0;
    item2.attachShadow({mode: 'open'});
    const actionBtn = document.createElement('button');
    actionBtn.id = 'actionButton';
    item2.shadowRoot!.appendChild(actionBtn);

    const item3 = document.createElement('div');
    item3.id = 'item3';
    item3.tabIndex = 0;

    root.appendChild(item1);
    root.appendChild(item2);
    root.appendChild(item3);

    return {root, items: [item1, item2, item3]};
  }

  function dispatchArrowEvent(element: HTMLElement, key: string) {
    element.dispatchEvent(new KeyboardEvent('keydown', {
      key,
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testDom = createTestDOM();
    rootElement = testDom.root;
    items = testDom.items;
    document.body.appendChild(rootElement);

    service = new KeyArrowNavigationService(rootElement, () => items);
    KeyArrowNavigationService.setInstance(service);
    service.startListening();
    service.rebuildNavigationElements();
  });

  teardown(() => {
    service.stopListening();
  });

  test('Initializes with navigable elements', () => {
    const elementIds =
        service.getElementsForTesting().map((el: HTMLElement) => el.id);
    assertArrayEquals(['item1', 'item2', 'item3'], elementIds);
    assertEquals(3, service.getElementCount());
    assertEquals(service, KeyArrowNavigationService.getInstance());
  });

  test('Moves focus down and up with arrow keys', () => {
    items[0]!.focus();
    assertEquals('item1', getDeepActiveElement()!.id);

    dispatchArrowEvent(items[0]!, 'ArrowDown');
    assertEquals('item2', getDeepActiveElement()!.id);

    dispatchArrowEvent(items[1]!, 'ArrowDown');
    assertEquals('item3', getDeepActiveElement()!.id);

    // Wraps around to first item
    dispatchArrowEvent(items[2]!, 'ArrowDown');
    assertEquals('item1', getDeepActiveElement()!.id);

    // Wraps backward to last item
    dispatchArrowEvent(items[0]!, 'ArrowUp');
    assertEquals('item3', getDeepActiveElement()!.id);

    dispatchArrowEvent(items[2]!, 'ArrowUp');
    assertEquals('item2', getDeepActiveElement()!.id);
  });

  test('Updates focus index on focusin', () => {
    items[2]!.focus();
    assertEquals(items[2], service.getElementAtFocusIndexForTesting());

    dispatchArrowEvent(items[2]!, 'ArrowDown');
    assertEquals('item1', getDeepActiveElement()!.id);
  });

  test('Navigates when focus is on menuButton or actionButton', () => {
    const menuBtn =
        items[0]!.shadowRoot!.querySelector<HTMLElement>('#menuButton')!;
    menuBtn.focus();

    dispatchArrowEvent(menuBtn, 'ArrowDown');
    assertEquals('item2', getDeepActiveElement()!.id);

    menuBtn.focus();
    dispatchArrowEvent(menuBtn, 'ArrowUp');
    assertEquals('item3', getDeepActiveElement()!.id);

    const actionBtn =
        items[1]!.shadowRoot!.querySelector<HTMLElement>('#actionButton')!;
    actionBtn.focus();

    dispatchArrowEvent(actionBtn, 'ArrowDown');
    assertEquals('item3', getDeepActiveElement()!.id);

    actionBtn.focus();
    dispatchArrowEvent(actionBtn, 'ArrowUp');
    assertEquals('item1', getDeepActiveElement()!.id);
  });

  test('Does not navigate when focus is inside a dialog', () => {
    const dialog = document.createElement('dialog');
    dialog.open = true;
    const dialogBtn = document.createElement('button');
    dialogBtn.id = 'dialogBtn';
    dialog.appendChild(dialogBtn);
    items[0]!.shadowRoot!.appendChild(dialog);

    dialogBtn.focus();
    dispatchArrowEvent(dialogBtn, 'ArrowDown');
    assertEquals('dialogBtn', getDeepActiveElement()!.id);
  });

  test('setCurrentFocusIndex updates focus index', () => {
    assertTrue(service.setCurrentFocusIndex(items[1]!));
    assertEquals(items[1], service.getElementAtFocusIndexForTesting());

    const outsideElement = document.createElement('div');
    assertFalse(service.setCurrentFocusIndex(outsideElement));
  });

  test(
      'rebuildNavigationElements clamps focus index if elements shrink', () => {
        items[2]!.focus();
        assertEquals(2, service.getElementsForTesting().indexOf(items[2]!));

        // Shrink items list
        items.pop();
        items.pop();
        service.rebuildNavigationElements();

        assertEquals(1, service.getElementCount());
        assertEquals(items[0], service.getElementAtFocusIndexForTesting());
      });
});
