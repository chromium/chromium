// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';

import type {CrTreeElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import type {CrTreeItemElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import { SELECTED_ATTR} from 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('cr-tree', function() {
  let tree: CrTreeElement;
  let root: CrTreeItemElement;
  let foo: CrTreeItemElement;
  let bar: CrTreeItemElement;
  let baz: CrTreeItemElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tree = document.createElement('cr-tree');
    document.body.appendChild(tree);

    root = document.createElement('cr-tree-item');
    root.label = 'Root';
    foo = document.createElement('cr-tree-item');
    foo.label = 'Foo';
    bar = document.createElement('cr-tree-item');
    bar.label = 'Bar';
    baz = document.createElement('cr-tree-item');
    baz.label = 'Baz';

    bar.add(baz);
    root.add(foo);
    root.add(bar);
    tree.add(root);
  });

  // Check tree structure is created correctly.
  test('items', () => {
    assertTrue(tree.hasChildren);
    assertEquals(1, tree.items.length);
    assertEquals(root, tree.items[0]);
    assertEquals(tree, root.tree);

    assertTrue(root.hasChildren);
    assertEquals(2, root.items.length);
    assertEquals(foo, root.items[0]);
    assertEquals(bar, root.items[1]);
    assertEquals(tree, root.tree);
    assertEquals(tree, root.parentItem);

    assertFalse(foo.hasChildren);
    assertEquals(0, foo.items.length);
    assertEquals(tree, foo.tree);
    assertEquals(root, foo.parentItem);

    assertTrue(bar.hasChildren);
    assertEquals(1, bar.items.length);
    assertEquals(baz, bar.items[0]);
    assertEquals(tree, bar.tree);
    assertEquals(root, bar.parentItem);

    assertFalse(baz.hasChildren);
    assertEquals(0, baz.items.length);
    assertEquals(tree, baz.tree);
    assertEquals(bar, baz.parentItem);
  });

  // Verify selecting an item expands its ancestors.
  test('selection', async () => {
    assertFalse(root.expanded);
    assertFalse(bar.expanded);
    assertFalse(foo.hasAttribute(SELECTED_ATTR));
    const whenExpand = eventToPromise('cr-tree-item-expand', tree);
    tree.selectedItem = foo;
    await whenExpand;
    assertTrue(foo.hasAttribute(SELECTED_ATTR));
    assertFalse(root.hasAttribute(SELECTED_ATTR));
    assertTrue(root.expanded);
    assertFalse(bar.hasAttribute(SELECTED_ATTR));
    assertFalse(bar.expanded);
  });

  test('collapse/expand with arrow keys', async () => {
    // Expand root
    tree.selectedItem = root;
    let whenExpand = eventToPromise('cr-tree-item-expand', tree);
    tree.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    await whenExpand;
    assertTrue(root.expanded);
    assertFalse(bar.expanded);

    // Select bar
    tree.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    tree.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    assertEquals(bar, tree.selectedItem);

    // Expand bar
    whenExpand = eventToPromise('cr-tree-item-expand', tree);
    tree.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight'}));
    await whenExpand;
    assertTrue(root.expanded);
    assertTrue(bar.expanded);

    // Re-select root
    tree.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    tree.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    assertEquals(root, tree.selectedItem);

    // Collapse root
    const whenCollapse = eventToPromise('cr-tree-item-collapse', tree);
    tree.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    await whenCollapse;
    assertFalse(root.expanded);
    assertTrue(bar.expanded);
  });

  test('removal', async () => {
    const whenExpand = eventToPromise('cr-tree-item-expand', tree);
    tree.selectedItem = baz;
    await whenExpand;
    assertTrue(bar.hasChildren);
    assertEquals(1, bar.items.length);
    assertFalse(bar.hasAttribute(SELECTED_ATTR));
    assertTrue(baz.hasAttribute(SELECTED_ATTR));
    const whenChange = eventToPromise('cr-tree-change', tree);
    bar.removeTreeItem(baz);
    await whenChange;
    assertEquals(bar, tree.selectedItem);
    assertTrue(bar.hasAttribute(SELECTED_ATTR));
    assertFalse(bar.hasChildren);
    assertEquals(0, bar.items.length);
  });

  test('expand on icon click', async () => {
    tree.selectedItem = root;
    assertFalse(root.expanded);
    let whenExpand = eventToPromise('cr-tree-item-expand', tree);
    const expand = root.shadowRoot!.querySelector<HTMLElement>('.expand-icon');
    assertTrue(!!expand);
    expand.click();
    await whenExpand;

    assertTrue(root.expanded);
    assertFalse(bar.expanded);
    whenExpand = eventToPromise('cr-tree-item-expand', tree);
    const barExpand =
        bar.shadowRoot!.querySelector<HTMLElement>('.expand-icon');
    assertTrue(!!barExpand);
    barExpand.click();

    assertTrue(root.expanded);
    assertTrue(bar.expanded);
    // Selection isn't impacted by clicking the expand icon.
    assertEquals(root, tree.selectedItem);
  });
});
