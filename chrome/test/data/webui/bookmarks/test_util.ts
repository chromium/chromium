// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksFolderNodeElement, FolderOpenState, NodeMap} from 'chrome://bookmarks/bookmarks.js';
import {normalizeNodes} from 'chrome://bookmarks/bookmarks.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

/**
 * Replace the current body of the test with a new element.
 */
export function replaceBody(element: Element) {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;

  window.history.replaceState({}, '', '/');

  document.body.appendChild(element);
}

/**
 * Convert a list of top-level bookmark nodes into a normalized lookup table of
 * nodes.
 */
export function testTree(...nodes: chrome.bookmarks.BookmarkTreeNode[]):
    NodeMap {
  return normalizeNodes(createFolder('0', nodes));
}

/**
 * Creates a folder with given properties.
 */
export function createFolder(
    id: string, children: chrome.bookmarks.BookmarkTreeNode[],
    config?: Partial<chrome.bookmarks.BookmarkTreeNode>):
    chrome.bookmarks.BookmarkTreeNode {
  const newFolder = Object.assign(
      {
        id: id,
        children: children,
        title: '',
      },
      config || {});

  if (children.length) {
    for (let i = 0; i < children.length; i++) {
      children[i]!.index = i;
      children[i]!.parentId = newFolder.id;
    }
  }
  return newFolder;
}

/**
 * Splices out the item/folder at |index| and adjusts the indices of all the
 * items after that.
 */
export function removeChild(
    tree: chrome.bookmarks.BookmarkTreeNode, index: number) {
  const children = tree.children!;
  children.splice(index, 1);
  for (let i = index; i < children.length; i++) {
    children[i]!.index = i;
  }
}

/**
 * Creates a bookmark with given properties.
 */
export function createItem(
    id: string, config?: Partial<chrome.bookmarks.BookmarkTreeNode>):
    chrome.bookmarks.BookmarkTreeNode {
  return Object.assign(
      {
        id: id,
        title: '',
        url: 'http://www.google.com/',
      },
      config || {});
}

export function normalizeIterable<T>(iterable: Iterable<T>): T[] {
  return Array.from(iterable).sort();
}

export function getAllFoldersOpenState(nodes: NodeMap): FolderOpenState {
  const folderOpenState = new Map();
  Object.keys(nodes).forEach((n) => folderOpenState.set(n, true));
  return folderOpenState;
}

/**
 * Sends a custom click event to |element|. All ctrl-clicks are automatically
 * rewritten to command-clicks on Mac.
 */
export function customClick(
    element: HTMLElement, config?: MouseEventInit, eventName?: string) {
  eventName = eventName || 'click';
  const props = Object.assign(
      {
        bubbles: true,
        cancelable: true,
        composed: true,
        button: 0,
        buttons: 1,
        shiftKey: false,
        ctrlKey: false,
        detail: 1,
      },
      config || {});

  if (isMac && props.ctrlKey) {
    props.ctrlKey = false;
    props.metaKey = true;
  }

  element.dispatchEvent(new MouseEvent('mousedown', props));
  element.dispatchEvent(new MouseEvent('mouseup', props));
  element.dispatchEvent(new MouseEvent(eventName, props));
  if (config && config.detail === 2) {
    element.dispatchEvent(new MouseEvent('dblclick', props));
  }
}

/**
 * Returns a folder node beneath |rootNode| which matches |id|.
 */
export function findFolderNode(
    rootNode: BookmarksFolderNodeElement,
    id: string): BookmarksFolderNodeElement|undefined {
  const nodes = [rootNode];
  let node;
  while (nodes.length) {
    node = nodes.pop()!;
    if (node.itemId === id) {
      return node;
    }

    node.shadowRoot!.querySelectorAll('bookmarks-folder-node').forEach((x) => {
      nodes.unshift(x);
    });
  }
  return undefined;
}

/**
 * Returns simple equivalents to chrome.test.* APIs for simple porting of
 * ExtensionAPITests.
 * @return {Object}
 */
export function simulateChromeExtensionAPITest() {
  const promises: Array<Promise<void>> = [];
  function pass(callback: Function) {
    let resolve: () => void;
    assertEquals(undefined, chrome.runtime.lastError);
    promises.push(new Promise<void>(r => {
      resolve = r;
    }));
    return function() {
      callback.apply(null, arguments);
      resolve();
    };
  }

  function fail(message: string) {
    let resolve: () => void;
    promises.push(new Promise<void>(r => {
      resolve = r;
    }));
    return function() {
      assertEquals(message, chrome.runtime.lastError!.message);
      chrome.runtime.lastError = undefined;
      resolve();
    };
  }

  async function runTests(tests: Function[]) {
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
