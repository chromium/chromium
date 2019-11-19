// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr.js
// require: cr/ui.js
// require: cr/ui/tree.js

(function() {
  /**
   * A helper function to determine if a node is the root of its type.
   *
   * @param {!Object} node The node to check.
   */
  function isTypeRootNode(node) {
    return node.PARENT_ID == 'r' && node.UNIQUE_SERVER_TAG != '';
  }

  /**
   * A helper function to determine if a node is a child of the given parent.
   *
   * @param {!Object} parent node.
   * @param {!Object} node The node to check.
   */
  function isChildOf(parentNode, node) {
    if (node.PARENT_ID != '') {
      return node.PARENT_ID == parentNode.ID;
    } else {
      return node.modelType == parentNode.modelType;
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
    } else if (nodeA.NON_UNIQUE_NAME != nodeB.NON_UNIQUE_NAME) {
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
    jstProcess(new JsEvalContext(node.entry_), nodeDetailsView);
  }

  /**
   * Updates the 'Last refresh time' display.
   * @param {string} str The text to display.
   */
  function setLastRefreshTime(str) {
    $('node-browser-refresh-time').textContent = str;
  }

  /**
   * Creates a new sync node tree item.
   *
   * @constructor
   * @param {!Object} node The nodeDetails object for the node as returned by
   *     chrome.sync.getAllNodes().
   * @extends {cr.ui.TreeItem}
   */
  function SyncNodeTreeItem(node) {
    const treeItem = new cr.ui.TreeItem();
    treeItem.__proto__ = SyncNodeTreeItem.prototype;

    treeItem.entry_ = node;
    treeItem.label = node.NON_UNIQUE_NAME;
    if (node.IS_DIR) {
      treeItem.mayHaveChildren_ = true;

      // Load children on expand.
      treeItem.expanded_ = false;
      treeItem.addEventListener('expand',
                                treeItem.handleExpand_.bind(treeItem));
    } else {
      treeItem.classList.add('leaf');
    }
    return treeItem;
  }

  SyncNodeTreeItem.prototype = {
    __proto__: cr.ui.TreeItem.prototype,

    /**
     * Finds the children of this node and appends them to the tree.
     */
    handleExpand_: function(event) {
      const treeItem = this;

      if (treeItem.expanded_) {
        return;
      }
      treeItem.expanded_ = true;

      const children = treeItem.tree.allNodes.filter(
          isChildOf.bind(undefined, treeItem.entry_));
      children.sort(nodeComparator);

      children.forEach(function(node) {
        treeItem.add(new SyncNodeTreeItem(node));
      });
    },
  };

  /**
   * Creates a new sync node tree.  Technically, it's a forest since it each
   * type has its own root node for its own tree, but it still looks and acts
   * mostly like a tree.
   *
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.Tree}
   */
  const SyncNodeTree = cr.ui.define('tree');

  SyncNodeTree.prototype = {
    __proto__: cr.ui.Tree.prototype,

    decorate: function() {
      cr.ui.Tree.prototype.decorate.call(this);
      this.addEventListener('change', this.handleChange_.bind(this));
      this.allNodes = [];
    },

    populate: function(nodes) {
      const tree = this;

      // We store the full set of nodes in the SyncNodeTree object.
      tree.allNodes = nodes;

      const roots = tree.allNodes.filter(isTypeRootNode);
      roots.sort(nodeComparator);

      roots.forEach(function(typeRoot) {
        tree.add(new SyncNodeTreeItem(typeRoot));
      });
    },

    handleChange_: function(event) {
      if (this.selectedItem) {
        updateNodeDetailView(this.selectedItem);
      }
    }
  };

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

  /**
   * Fetch the latest set of nodes and refresh the UI.
   */
  function refresh() {
    $('node-browser-refresh-button').disabled = true;

    clear();
    setLastRefreshTime('In progress since ' + (new Date()).toLocaleString());

    chrome.sync.getAllNodes(function(nodeMap) {
      // Put all nodes into one big list that ignores the type.
      const nodes = nodeMap
                        .map(function(x) {
                          return x.nodes;
                        })
                        .reduce(function(a, b) {
                          return a.concat(b);
                        });

      const treeContainer = $('sync-node-tree-container');
      const tree = document.createElement('tree');
      tree.setAttribute('id', 'sync-node-tree');
      tree.setAttribute('icon-visibility', 'parent');
      treeContainer.appendChild(tree);

      cr.ui.decorate(tree, SyncNodeTree);
      tree.populate(nodes);

      setLastRefreshTime((new Date()).toLocaleString());
      $('node-browser-refresh-button').disabled = false;
    });
  }

  document.addEventListener('DOMContentLoaded', function(e) {
    $('node-browser-refresh-button').addEventListener('click', refresh);
    const Splitter = cr.ui.Splitter;
    const customSplitter = cr.ui.define('div');

    customSplitter.prototype = {
      __proto__: Splitter.prototype,

      handleSplitterDragEnd: function(e) {
        Splitter.prototype.handleSplitterDragEnd.apply(this, arguments);
        const treeElement = $('sync-node-tree-container');
        const newWidth = parseFloat(treeElement.style.width);
        treeElement.style.minWidth = Math.max(newWidth, 50) + "px";
      }
    };

    customSplitter.decorate($("sync-node-splitter"));

    // Automatically trigger a refresh the first time this tab is selected.
    $('sync-browser-tab').addEventListener('selectedChange', function f(e) {
      if (this.selected) {
        $('sync-browser-tab').removeEventListener('selectedChange', f);
        refresh();
      }
    });
  });

})();
