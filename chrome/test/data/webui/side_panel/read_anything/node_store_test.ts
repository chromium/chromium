// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, NodeStore} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('NodeStore', () => {
  let nodeStore: NodeStore;
  let readingMode: FakeReadingMode;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    nodeStore = new NodeStore();
  });

  test('setDomNode', () => {
    const node = document.createElement('p');
    const id = 308;

    nodeStore.setDomNode(node, id);

    assertEquals(id, nodeStore.getAxId(node));
    assertEquals(node, nodeStore.getDomNode(id));
  });

  test('removeDomNode after set', () => {
    const node1 = document.createElement('p');
    const id1 = 308;
    const node2 = document.createElement('span');
    const id2 = 310;

    nodeStore.setDomNode(node1, id1);
    nodeStore.setDomNode(node2, id2);
    nodeStore.removeDomNode(node1);

    assertFalse(!!nodeStore.getAxId(node1));
    assertFalse(!!nodeStore.getDomNode(id1));
    assertEquals(id2, nodeStore.getAxId(node2));
    assertEquals(node2, nodeStore.getDomNode(id2));
  });

  test('removeDomNode without setting does not crash', () => {
    nodeStore.setDomNode(document.createElement('b'), 124);
  });

  test('clearDomNodes after set', () => {
    const node1 = document.createElement('p');
    const id1 = 308;
    const node2 = document.createElement('span');
    const id2 = 310;
    nodeStore.setDomNode(node1, id1);
    nodeStore.setDomNode(node2, id2);

    nodeStore.clearDomNodes();

    assertFalse(!!nodeStore.getAxId(node1));
    assertFalse(!!nodeStore.getDomNode(id1));
    assertFalse(!!nodeStore.getAxId(node2));
    assertFalse(!!nodeStore.getDomNode(id2));
  });

  test('replaceDomNode', () => {
    const parent = document.createElement('p');
    const parentId = 308;
    const child = document.createTextNode('You can build me up');
    const childId = 310;
    parent.appendChild(child);
    nodeStore.setDomNode(parent, parentId);
    nodeStore.setDomNode(child, childId);
    const replacer = document.createTextNode('You can tear me down');

    nodeStore.replaceDomNode(child, replacer);

    assertEquals(replacer, nodeStore.getDomNode(childId));
    assertEquals(parent, nodeStore.getDomNode(parentId));
    assertFalse(!!nodeStore.getAxId(child));
    const children = parent.childNodes;
    assertEquals(1, children.length);
    assertEquals(replacer, children.item(0));
  });

  test('hideImageNode', () => {
    const id = 216;
    nodeStore.hideImageNode(id);
    assertTrue(nodeStore.areNodesAllHidden([id]));
  });

  test('areNodesAllHidden', () => {
    const id1 = 216;
    const id2 = 218;
    const id3 = 219;
    nodeStore.hideImageNode(id1);
    nodeStore.hideImageNode(id2);

    assertFalse(nodeStore.areNodesAllHidden([id1, id2, id3]));
    assertTrue(nodeStore.areNodesAllHidden([id1, id2]));
    assertFalse(nodeStore.areNodesAllHidden([id1, id3]));
  });

  test('addImageToFetch', () => {
    nodeStore.addImageToFetch(216);
    assertTrue(nodeStore.hasImagesToFetch());
  });

  test('fetchImages', () => {
    const id1 = 4002;
    const id2 = 4003;
    const id3 = 4004;
    nodeStore.addImageToFetch(id1);
    nodeStore.addImageToFetch(id2);
    nodeStore.addImageToFetch(id3);

    nodeStore.fetchImages();

    assertArrayEquals([id1, id2, id3], readingMode.fetchedImages);
    assertFalse(nodeStore.hasImagesToFetch());
  });
});
