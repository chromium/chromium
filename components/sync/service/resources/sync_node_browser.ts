// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_splitter/cr_splitter.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';

import type {CrTreeItemElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {SyncNode, SyncNodeMap} from './chrome_sync.js';
import {getAllNodes} from './chrome_sync.js';

/**
 * A helper function to determine if a node is the root of its type.
 */
function isTypeRootNode(node: SyncNode): boolean {
  return node.PARENT_ID === 'r' && node.UNIQUE_SERVER_TAG !== '';
}

/**
 * A helper function to determine if a node is a child of the given parent.
 */
function isChildOf(parentNode: SyncNode, node: SyncNode) {
  if (node.PARENT_ID !== '') {
    return node.PARENT_ID === parentNode.ID;
  } else {
    return node.dataType === parentNode.dataType;
  }
}

/**
 * A helper function to sort sync nodes.
 *
 * Sorts by position index if possible, falls back to sorting by name, and
 * finally sorting by METAHANDLE.
 *
 * If this proves to be slow and expensive, we should experiment with moving
 * this functionality to C++ instead.
 */
function nodeComparator(nodeA: SyncNode, nodeB: SyncNode): number {
  if (nodeA.hasOwnProperty('positionIndex') &&
      nodeB.hasOwnProperty('positionIndex')) {
    return nodeA.positionIndex! - nodeB.positionIndex!;
  } else if (nodeA.NON_UNIQUE_NAME !== nodeB.NON_UNIQUE_NAME) {
    return nodeA.NON_UNIQUE_NAME.localeCompare(nodeB.NON_UNIQUE_NAME);
  } else {
    return nodeA.METAHANDLE - nodeB.METAHANDLE;
  }
}

/**
 * Updates the node detail view with the details for the given node.
 * @param node The struct representing the node we want to display.
 */
function updateNodeDetailView(node: CrTreeItemElement) {
  const nodeDetailsView = document.querySelector<HTMLElement>('#node-details');
  assert(nodeDetailsView);
  nodeDetailsView.hidden = false;
  const detail = node.detail as {payload: SyncNode};
  jstProcess(new JsEvalContext(detail.payload), nodeDetailsView);
}

/**
 * Updates the 'Last refresh time' display.
 * @param str The text to display.
 */
function setLastRefreshTime(str: string) {
  const refreshTime =
      document.querySelector<HTMLElement>('#node-browser-refresh-time');
  assert(refreshTime);
  refreshTime.textContent = str;
}

/**
 * Clears any existing UI state.  Useful prior to a refresh.
 */
function clear() {
  const treeContainer =
      document.querySelector<HTMLElement>('#sync-node-tree-container');
  assert(treeContainer);
  while (treeContainer.firstChild) {
    treeContainer.removeChild(treeContainer.firstChild);
  }

  const nodeDetailsView = document.querySelector<HTMLElement>('#node-details');
  assert(nodeDetailsView);
  nodeDetailsView.hidden = true;
}

function setNode(treeItem: CrTreeItemElement, node: SyncNode) {
  (treeItem.detail as {payload: SyncNode}).payload = node;
  treeItem.label = node.NON_UNIQUE_NAME;
  if (node.IS_DIR) {
    treeItem.toggleAttribute('may-have-children', true);

    // Load children on expand.
    treeItem.toggleAttribute('expanded', false);
    treeItem.addEventListener(
        'cr-tree-item-expand', () => handleExpand(treeItem));
  } else {
    treeItem.classList.add('leaf');
  }
}

function handleExpand(treeItem: CrTreeItemElement) {
  if (treeItem.hasChildren) {
    return;
  }

  const treeItemData = (treeItem.detail as {payload: SyncNode}).payload;
  const treeData = (treeItem.tree!.detail as {payload: SyncNode[]}).payload;
  const children = treeData.filter(node => isChildOf(treeItemData, node));
  children.sort(nodeComparator);

  children.forEach(function(node: SyncNode) {
    const item = document.createElement('cr-tree-item');
    treeItem.add(item);
    setNode(item, node);
  });
}

/**
 * Fetch the latest set of nodes and refresh the UI.
 */
function refresh() {
  const refreshButton =
      document.querySelector<HTMLButtonElement>('#node-browser-refresh-button');
  assert(refreshButton);
  refreshButton.disabled = true;

  clear();
  setLastRefreshTime('In progress since ' + (new Date()).toLocaleString());

  getAllNodes(function(nodeMap: SyncNodeMap) {
    let nodes: SyncNode[] = [];
    if (nodeMap && nodeMap.length > 0) {
      // Put all nodes into one big list that ignores the type.
      nodes = nodeMap.map(x => x.nodes).reduce((a, b) => a.concat(b));
    }

    const treeContainer =
        document.querySelector<HTMLElement>('#sync-node-tree-container');
    assert(treeContainer);
    const tree = document.createElement('cr-tree');
    tree.id = 'sync-node-tree';
    tree.addEventListener('cr-tree-change', () => {
      if (tree.selectedItem) {
        updateNodeDetailView(tree.selectedItem as CrTreeItemElement);
      }
    });
    treeContainer.appendChild(tree);

    tree.detail = {payload: nodes, children: {}};
    const roots = nodes.filter(isTypeRootNode);
    roots.sort(nodeComparator);
    roots.forEach(typeRoot => {
      const child = document.createElement('cr-tree-item');
      tree.add(child);
      setNode(child, typeRoot);
    });

    setLastRefreshTime((new Date()).toLocaleString());
    refreshButton.disabled = false;
  });
}

document.addEventListener('DOMContentLoaded', () => {
  const refreshButton =
      document.querySelector<HTMLButtonElement>('#node-browser-refresh-button');
  assert(refreshButton);
  refreshButton.addEventListener('click', refresh);
  const splitter = document.querySelector<HTMLElement>('#sync-node-splitter');
  assert(splitter);
  splitter.addEventListener('resize', () => {
    const treeElement =
        document.querySelector<HTMLElement>('#sync-node-tree-container');
    assert(treeElement);
    const newWidth = parseFloat(treeElement.style.width);
    treeElement.style.minWidth = Math.max(newWidth, 50) + 'px';
  });

  // Automatically trigger a refresh the first time this tab is selected.
  const tabBox = document.querySelector('cr-tab-box');
  assert(tabBox);
  tabBox.addEventListener('selected-index-change', function f() {
    const syncBrowserTab =
        document.querySelector<HTMLElement>('#sync-browser-tab');
    assert(syncBrowserTab);
    if (syncBrowserTab.hasAttribute('selected')) {
      assert(tabBox);
      tabBox.removeEventListener('selected-index-change', f);
      refresh();
    }
  });
});
