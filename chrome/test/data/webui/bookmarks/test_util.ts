// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksFolderNodeElement, FolderOpenState, MojoBookmarkNode, MojoRootNode, NodeMap} from 'chrome://bookmarks/bookmarks.js';
import {normalizeNodes, PermanentFolderType, ROOT_NODE_ID} from 'chrome://bookmarks/bookmarks.js';
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
export function testTree(...nodes: MojoBookmarkNode[]): NodeMap {
  return normalizeNodes(createRoot(nodes));
}

/**
 * Creates a root node with given children.
 */
export function createRoot(children: MojoBookmarkNode[]): MojoRootNode {
  return {
    id: {value: ROOT_NODE_ID},
    children: children.map(c => c.folder!),
  };
}

/**
 * Creates a folder with given properties.
 */
export function createFolder(
    id: string, children: MojoBookmarkNode[], config?: {
      isSynced?: boolean,
      permanentFolderType?: unknown, [key: string]: unknown,
    }): MojoBookmarkNode {
  let permanentFolderType = PermanentFolderType.kUnknown;
  const configFolderType =
      config?.permanentFolderType || config?.['folderType'];
  if (configFolderType !== undefined) {
    const ft = configFolderType;
    if (ft === PermanentFolderType.kBookmarkBar ||
        ft === chrome.bookmarks.FolderType.BOOKMARKS_BAR) {
      permanentFolderType = PermanentFolderType.kBookmarkBar;
    } else if (
        ft === PermanentFolderType.kOther ||
        ft === chrome.bookmarks.FolderType.OTHER) {
      permanentFolderType = PermanentFolderType.kOther;
    } else if (
        ft === PermanentFolderType.kMobile ||
        ft === chrome.bookmarks.FolderType.MOBILE) {
      permanentFolderType = PermanentFolderType.kMobile;
    } else if (
        ft === PermanentFolderType.kManaged ||
        ft === chrome.bookmarks.FolderType.MANAGED) {
      permanentFolderType = PermanentFolderType.kManaged;
    }
  } else if (config?.['unmodifiable'] === 'managed') {
    permanentFolderType = PermanentFolderType.kManaged;
  }

  let isSynced = true;
  if (config?.isSynced !== undefined) {
    isSynced = config.isSynced;
  } else if (config?.['syncing'] !== undefined) {
    isSynced = config['syncing'] as boolean;
  }

  let legacyId: bigint|null = null;
  try {
    legacyId = BigInt(id);
  } catch (e) {
    legacyId = null;
  }

  const folderData = Object.assign(
      {
        id: {value: id},
        children: children,
        title: '',
        isSynced: isSynced,
        permanentFolderType: permanentFolderType,
        legacy: legacyId !== null ? {id: legacyId} : null,
      },
      config || {});
  folderData.permanentFolderType = permanentFolderType;
  return {
    folder: folderData,
  };
}

/**
 * Splices out the item/folder at |index|.
 */
export function removeChild(tree: MojoBookmarkNode, index: number) {
  const children = tree.folder!.children;
  children.splice(index, 1);
}

/**
 * Creates a bookmark with given properties.
 */
export function createItem(id: string, config?: {
  isSynced?: boolean,
  url?: string,
  title?: string, [key: string]: unknown,
}): MojoBookmarkNode {
  let isSynced = true;
  if (config?.isSynced !== undefined) {
    isSynced = config.isSynced;
  } else if (config?.['syncing'] !== undefined) {
    isSynced = config['syncing'] as boolean;
  }

  let legacyId: bigint|null = null;
  try {
    legacyId = BigInt(id);
  } catch (e) {
    legacyId = null;
  }

  return {
    url: Object.assign(
        {
          id: {value: id},
          title: '',
          url: 'http://www.google.com/',
          faviconUrl: null,
          isSynced: isSynced,
          legacy: legacyId !== null ? {id: legacyId} : null,
        },
        config || {}),
  };
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

    node.shadowRoot.querySelectorAll('bookmarks-folder-node').forEach((x) => {
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
