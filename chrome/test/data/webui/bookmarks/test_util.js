// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac} from 'chrome://resources/js/cr.m.js';
import {normalizeNodes} from 'chrome://bookmarks/bookmarks.js';

/**
 * Replace the current body of the test with a new element.
 * @param {Element} element
 */
export function replaceBody(element) {
  PolymerTest.clearBody();

  window.history.replaceState({}, '', '/');

  document.body.appendChild(element);
}

/**
 * Convert a list of top-level bookmark nodes into a normalized lookup table of
 * nodes.
 * @param {...BookmarkTreeNode} nodes
 * @return {NodeMap}
 */
export function testTree(nodes) {
  return normalizeNodes(
      createFolder('0', Array.from(arguments)));
}

/**
 * Creates a folder with given properties.
 * @param {string} id
 * @param {Array<BookmarkTreeNode>} children
 * @param {Object=} config
 * @return {BookmarkTreeNode}
 */
export function createFolder(id, children, config) {
  const newFolder = {
    id: id,
    children: children,
    title: '',
  };
  if (config) {
    for (const key in config) {
      newFolder[key] = config[key];
    }
  }
  if (children.length) {
    for (let i = 0; i < children.length; i++) {
      children[i].index = i;
      children[i].parentId = newFolder.id;
    }
  }
  return newFolder;
}

/**
 * Splices out the item/folder at |index| and adjusts the indices of all the
 * items after that.
 * @param {BookmarkTreeNode} tree
 * @param {number} index
 */
export function removeChild(tree, index) {
  tree.children.splice(index, 1);
  for (let i = index; i < tree.children.length; i++) {
    tree.children[i].index = i;
  }
}

/**
 * Creates a bookmark with given properties.
 * @param {string} id
 * @param {Object=} config
 * @return {BookmarkTreeNode}
 */
export function createItem(id, config) {
  const newItem = {
    id: id,
    title: '',
    url: 'http://www.google.com/',
  };
  if (config) {
    for (const key in config) {
      newItem[key] = config[key];
    }
  }
  return newItem;
}

/**
 * @param {Set<T>|Map<T>}
 * @return {Array<T>}
 * @template T
 */
export function normalizeIterable(iterable) {
  return Array.from(iterable).sort();
}

/**
 * @param {NodeState} nodes
 * @return {FolderOpenState}
 */
export function getAllFoldersOpenState(nodes) {
  const folderOpenState = new Map();
  Object.keys(nodes).forEach((n) => folderOpenState.set(n, true));
  return folderOpenState;
}

/**
 * Sends a custom click event to |element|. All ctrl-clicks are automatically
 * rewritten to command-clicks on Mac.
 * @param {HTMLElement} element
 * @param {Object=} config
 * @param {string=} eventName
 */
export function customClick(element, config, eventName) {
  eventName = eventName || 'click';
  const props = {
    bubbles: true,
    cancelable: true,
    composed: true,
    button: 0,
    buttons: 1,
    shiftKey: false,
    ctrlKey: false,
    detail: 1,
  };

  if (config) {
    for (const key in config) {
      props[key] = config[key];
    }
  }

  if (isMac && props.ctrlKey) {
    props.ctrlKey = false;
    props.metaKey = true;
  }

  element.dispatchEvent(new MouseEvent('mousedown', props));
  element.dispatchEvent(new MouseEvent('mouseup', props));
  element.dispatchEvent(new MouseEvent(eventName, props));
  if (config && config.detail == 2) {
    element.dispatchEvent(new MouseEvent('dblclick', props));
  }
}

/**
 * Returns a folder node beneath |rootNode| which matches |id|.
 * @param {BookmarksFolderNodeElement} rootNode
 * @param {string} id
 * @return {BookmarksFolderNodeElement}
 */
export function findFolderNode(rootNode, id) {
  const nodes = [rootNode];
  let node;
  while (nodes.length) {
    node = nodes.pop();
    if (node.itemId == id) {
      return node;
    }

    node.root.querySelectorAll('bookmarks-folder-node').forEach((x) => {
      nodes.unshift(x);
    });
  }
}

/**
 * Returns simple equivalents to chrome.test.* APIs for simple porting of
 * ExtensionAPITests.
 * @return {Object}
 */
export function simulateChromeExtensionAPITest() {
  const promises = [];
  function pass(callback) {
    let resolve;
    assertEquals(undefined, chrome.runtime.lastError);
    promises.push(new Promise(r => {
      resolve = r;
    }));
    return function() {
      callback.apply(null, arguments);
      resolve();
    };
  }

  function fail(message) {
    let resolve;
    promises.push(new Promise(r => {
      resolve = r;
    }));
    return function() {
      assertEquals(message, chrome.runtime.lastError.message);
      chrome.runtime.lastError = undefined;
      resolve();
    };
  }

  async function runTests(tests) {
    for (const test of tests) {
      test();
      await Promise.all(promises);
    }
  }
  return {
    pass,
    fail,
    runTests,
  };
}
