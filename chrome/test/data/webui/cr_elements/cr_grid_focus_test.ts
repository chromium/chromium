// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';

import type {CrGridElement} from 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('CrElementsGridFocusTest', () => {
  function createGrid(
      size: number, disableArrowNav: boolean = false): CrGridElement {
    const grid = document.createElement('cr-grid');
    for (let i = 0; i < size; i++) {
      const div = document.createElement('div');
      div.tabIndex = 0;
      div.id = i.toString();
      grid.appendChild(div);
    }
    grid.columns = 6;
    grid.disableArrowNavigation = disableArrowNav;
    document.body.appendChild(grid);
    return grid;
  }

  /**
   * Asserts that an element is focused.
   */
  function assertFocus(element: Element) {
    assertEquals(element, getDeepActiveElement());
  }

  function keydown(element: Element, key: string) {
    keyDownOn(element, 0, [], key);
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('right focuses right item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[0]!, 'ArrowRight');

    // Assert.
    assertFocus(grid.children[1]!);
  });

  test('right wrap around focuses first item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[grid.children.length - 1]!, 'ArrowRight');

    // Assert.
    assertFocus(grid.children[0]!);
  });

  test('left focuses left item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[1]!, 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[0]!);
  });

  test('left wrap around focuses last item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[0]!, 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[grid.children.length - 1]!);
  });

  test('right focuses left item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[1]!, 'ArrowRight');

    // Assert.
    assertFocus(grid.children[0]!);
  });

  test('right wrap around focuses last item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[0]!, 'ArrowRight');

    // Assert.
    assertFocus(grid.children[grid.children.length - 1]!);
  });

  test('left focuses right item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[0]!, 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[1]!);
  });

  test('left wrap around focuses first item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[grid.children.length - 1]!, 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[0]!);
  });


  const test_params = [
    {
      type: 'full',
      size: 10,
      columns: 5,
    },
    {
      type: 'non-full',
      size: 8,
      columns: 5,
    },
  ];

  test_params.forEach(function(param) {
    test(`down focuses below item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[0]!, 'ArrowDown');

      // Assert.
      assertFocus(grid.children[param.columns]!);
    });

    test(`last column down focuses below item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns - 1]!, 'ArrowDown');

      // Assert.
      const focusedIndex =
          (param.size % param.columns === 0 ? param.size : param.columns) - 1;
      assertFocus(grid.children[focusedIndex]!);
    });

    test(`up focuses above item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns]!, 'ArrowUp');

      // Assert.
      assertFocus(grid.children[0]!);
    });

    test(`last column up focuses above item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns - 1]!, 'ArrowUp');

      // Assert.
      const focusedIndex =
          (param.size % param.columns === 0 ? param.size : param.columns) - 1;
      assertFocus(grid.children[focusedIndex]!);
    });

    test(`down wrap around focuses top item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns]!, 'ArrowDown');

      // Assert.
      assertFocus(grid.children[0]!);
    });

    test(`up wrap around focuses bottom item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[0]!, 'ArrowDown');

      // Assert.
      assertFocus(grid.children[param.columns]!);
    });
  });

  test('enter clicks focused item', async () => {
    // Arrange.
    const grid = createGrid(1);
    (grid.children[0] as HTMLElement).focus();
    const itemClicked = eventToPromise('click', grid.children[0]!);

    // Act.
    keydown(grid.children[0]!, 'Enter');

    // Assert.
    await itemClicked;
  });

  test('space clicks focused item', async () => {
    // Arrange.
    const grid = createGrid(1);
    (grid.children[0] as HTMLElement).focus();
    const itemClicked = eventToPromise('click', grid.children[0]!);

    // Act.
    keydown(grid.children[0]!, ' ');

    // Assert.
    await itemClicked;
  });

  test('right arrow does not change focus with arrow nav disabled', () => {
    // Arrange.
    const grid = createGrid(12, true);

    // Act.
    keydown(grid.children[0]!, 'ArrowRight');

    // Assert.
    assertFocus(document.body);
  });

  test('left arrow does not change focus with arrow nav disabled', () => {
    // Arrange.
    const grid = createGrid(12, true);

    // Act.
    keydown(grid.children[0]!, 'ArrowLeft');

    // Assert.
    assertFocus(document.body);
  });

  test('up arrow does not change focus with arrow nav disabled', () => {
    // Arrange.
    const grid = createGrid(12, true);

    // Act.
    keydown(grid.children[0]!, 'ArrowUp');

    // Assert.
    assertFocus(document.body);
  });

  test('down arrow does not change focus with arrow nav disabled', () => {
    // Arrange.
    const grid = createGrid(12, true);

    // Act.
    keydown(grid.children[0]!, 'ArrowDown');

    // Assert.
    assertFocus(document.body);
  });

  test('ignoreModifiedKeyEvents', function() {
    const grid = createGrid(3);
    grid.ignoreModifiedKeyEvents = true;

    const items = grid.$.items.assignedElements();
    (items[0] as HTMLElement).focus();
    assertFocus(items[0]!);

    function assertFocusUnchanged(key: 'ArrowRight'|'ArrowLeft') {
      assertFocus(items[0]!);
      keyDownOn(items[0]!, 0, 'alt', key);
      assertFocus(items[0]!);
      keyDownOn(items[0]!, 0, 'ctrl', key);
      assertFocus(items[0]!);
      keyDownOn(items[0]!, 0, 'meta', key);
      assertFocus(items[0]!);
      keyDownOn(items[0]!, 0, 'shift', key);
      assertFocus(items[0]!);
    }

    // Test non-modifier events still work.
    keydown(items[0]!, 'ArrowRight');
    assertFocus(items[1]!);
    keydown(items[1]!, 'ArrowLeft');
    assertFocus(items[0]!);

    // Test modifier events don't change focus.
    assertFocusUnchanged('ArrowRight');
    assertFocusUnchanged('ArrowLeft');

    // Test RTL case.
    grid.dir = 'rtl';
    assertFocusUnchanged('ArrowRight');
    assertFocusUnchanged('ArrowLeft');

    keydown(items[0]!, 'ArrowRight');
    assertFocus(items[2]!);
    keydown(items[2]!, 'ArrowLeft');
    assertFocus(items[0]!);
  });

  // Test cases where keyboard events are coming from children of the slotted
  // elements and ensure that keyboard navigation still works.
  test('focusSelector focuses right item', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-grid focus-selector="button">
        <div><button id="0"></button></div>
        <div><button id="1"></button></div>
        <div><button id="2"></button></div>
      </cr-grid>`;

    const grid = document.body.querySelector('cr-grid')!;
    assertEquals('button', grid.focusSelector);
    const items = grid.$.items.assignedElements();
    const focusableChildren = items.map(i => i.querySelector('button'));

    // Focus first element.
    focusableChildren[0]!.focus();
    assertFocus(focusableChildren[0]!);

    // Navigate via keyboard.
    keydown(focusableChildren[0]!, 'ArrowRight');
    assertFocus(focusableChildren[1]!);

    keydown(focusableChildren[1]!, 'ArrowRight');
    assertFocus(focusableChildren[2]!);

    keydown(focusableChildren[2]!, 'ArrowRight');
    assertFocus(focusableChildren[0]!);
  });
});
