// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAloudNode, ReadAloudNodeStore} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('ReadAloudNodeStore', () => {
  let nodeStore: ReadAloudNodeStore;

  function assertDomNodesEqual(
      domNode: Node, readAloudNode: ReadAloudNode|undefined) {
    assertTrue(!!readAloudNode);
    const readAloudDomNode = readAloudNode.domNode();
    assertTrue(!!readAloudDomNode);
    assertEquals(domNode, readAloudDomNode);
  }

  setup(() => {
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isTsTextSegmentationEnabled = true;
    nodeStore = new ReadAloudNodeStore();
    ReadAloudNodeStore.setInstance(nodeStore);
  });

  test('registering a node adds it to the store', () => {
    const domNode = document.createElement('p');
    const readAloudNode = ReadAloudNode.create(domNode);
    assertTrue(!!readAloudNode);

    // The node should have been registered on creation.
    // To test this, we can try to update it and see if it works.
    const replacer = document.createElement('span');
    nodeStore.update(domNode, replacer);

    assertDomNodesEqual(replacer, readAloudNode);
  });

  test('update changes the dom node of the read aloud node', () => {
    const domNode = document.createElement('p');
    const readAloudNode = ReadAloudNode.create(domNode);
    assertDomNodesEqual(domNode, readAloudNode);

    const replacer = document.createElement('span');
    nodeStore.update(domNode, replacer);

    assertDomNodesEqual(replacer, readAloudNode);
  });

  test('update changes multiple read aloud nodes for the same dom node', () => {
    const domNode = document.createElement('p');
    const readAloudNode1 = ReadAloudNode.create(domNode);
    const readAloudNode2 = ReadAloudNode.create(domNode);
    assertDomNodesEqual(domNode, readAloudNode1);
    assertDomNodesEqual(domNode, readAloudNode2);

    const replacer = document.createElement('span');
    nodeStore.update(domNode, replacer);

    assertDomNodesEqual(replacer, readAloudNode1);
    assertDomNodesEqual(replacer, readAloudNode2);
  });

  test('update with unregistered node does not throw error', () => {
    const domNode = document.createElement('p');
    const replacer = document.createElement('span');
    // This shouldn't throw an error.
    nodeStore.update(domNode, replacer);
  });
});
