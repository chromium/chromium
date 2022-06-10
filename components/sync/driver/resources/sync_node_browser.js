// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';

import {define as crUiDefine} from 'chrome://resources/js/cr/ui.m.js';
import {Splitter} from 'chrome://resources/js/cr/ui/splitter.js';
import {$} from 'chrome://resources/js/util.m.js';

import {getAllNodes} from './chrome_sync.js';

/**
 * A helper function to determine if a node is the root of its type.
 *
 * @param {!Object} node The node to check.
 */
function isTypeRootNode(node) {
  return node.PARENT_ID === 'r' && node.UNIQUE_SERVER_TAG !== '';
}

/**
 * A helper function to determine if a node is a child of the given parent.
 *
 * @param {!Object} parent node.
 * @param {!Object} node The node to check.
 */
function isChildOf(parentNode, node) {
  if (node.PARENT_ID !== '') {
    return node.PARENT_ID === parentNode.ID;
  } else {
    return node.modelType === parentNode.modelType;
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
function nodeComparator(nodeA, nodeB) {
  if (nodeA.hasOwnProperty('positionIndex') &&
      nodeB.hasOwnProperty('positionIndex')) {
    return nodeA.positionIndex - nodeB.positionIndex;
  } else if (nodeA.NON_UNIQUE_NAME !== nodeB.NON_UNIQUE_NAME) {
    return nodeA.NON_UNIQUE_NAME.localeCompare(nodeB.NON_UNIQUE_NAME);
  } else {
    return nodeA.METAHANDLE - nodeB.METAHANDLE;
  }
}

/**
 * Updates the node detail view with the details for the given node.
 * @param {!Object} node The struct representing the node we want to display.
 */
function updateNodeDetailView(node) {
  const nodeDetailsView = $('node-details');
  nodeDetailsView.hidden = false;
  jstProcess(new JsEvalContext(node.detail.payload), nodeDetailsView);
}

/**
 * Updates the 'Last refresh time' display.
 * @param {string} str The text to display.
 */
function setLastRefreshTime(str) {
  $('node-browser-refresh-time').textContent = str;
}

/**
 * Clears any existing UI state.  Useful prior to a refresh.
 */
function clear() {
  const treeContainer = $('sync-node-tree-container');
  while (treeContainer.firstChild) {
    treeContainer.removeChild(treeContainer.firstChild);
  }

  const nodeDetailsView = $('node-details');
  nodeDetailsView.hidden = true;
}

function setNode(treeItem, node) {
  treeItem.detail.payload = node;
  treeItem.label = node.NON_UNIQUE_NAME;
  if (node.IS_DIR) {
    treeItem.toggleAttribute('may-have-children', true);

    // Load children on expand.
    treeItem.toggleAttribute('expanded', false);
    treeItem.addEventListener('cr-tree-item-expand', handleExpand(treeItem));
  } else {
    treeItem.classList.add('leaf');
  }
}

function handleExpand(treeItem) {
  if (treeItem.hasChildren) {
    return;
  }

  const treeItemData = treeItem.detail.payload;
  const treeData = treeItem.tree.detail.payload;
  const children = treeData.filter(node => isChildOf(treeItemData, node));
  children.sort(nodeComparator);

  children.forEach(function(node) {
    const item = document.createElement('cr-tree-item');
    treeItem.add(item);
    setNode(item, node);
  });
}

/**
 * Fetch the latest set of nodes and refresh the UI.
 */
function refresh() {
  $('node-browser-refresh-button').disabled = true;

  clear();
  setLastRefreshTime('In progress since ' + (new Date()).toLocaleString());

  getAllNodes(function(nodeMap) {
    let nodes = [];
    if (nodeMap && nodeMap.length > 0) {
      // Put all nodes into one big list that ignores the type.
      nodes = nodeMap.map(x => x.nodes).reduce((a, b) => a.concat(b));
    }

    const treeContainer = $('sync-node-tree-container');
    const tree = document.createElement('cr-tree');
    tree.id = 'sync-node-tree';
    tree.addEventListener('cr-tree-change', () => {
      if (tree.selectedItem) {
        updateNodeDetailView(tree.selectedItem);
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
    $('node-browser-refresh-button').disabled = false;
  });
}

document.addEventListener('DOMContentLoaded', function(e) {
  $('node-browser-refresh-button').addEventListener('click', refresh);
  const customSplitter = crUiDefine('div');

  customSplitter.prototype = {
    __proto__: Splitter.prototype,

    handleSplitterDragEnd(e) {
      Splitter.prototype.handleSplitterDragEnd.apply(this, arguments);
      const treeElement = $('sync-node-tree-container');
      const newWidth = parseFloat(treeElement.style.width);
      treeElement.style.minWidth = Math.max(newWidth, 50) + 'px';
    }
  };

  customSplitter.decorate($('sync-node-splitter'));

  // Automatically trigger a refresh the first time this tab is selected.
  document.querySelector('cr-tab-box')
      .addEventListener('selected-index-change', function f(e) {
        if (document.querySelector('#sync-browser-tab')
                .hasAttribute('selected')) {
          document.querySelector('cr-tab-box')
              .removeEventListener('selected-index-change', f);
          refresh();
        }
      });
});
