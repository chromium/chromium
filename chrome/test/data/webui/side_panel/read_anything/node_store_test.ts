// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, COUNT_WORDS_SEEN_DELAY_MS, NodeStore} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

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

  test('estimateWordsSeenWithDelay updates words seen after delay', () => {
    const node = document.createElement('p');
    const text = document.createTextNode('Annie are you okay?');
    nodeStore.setDomNode(node, 1);
    nodeStore.setDomNode(text, 2);
    node.appendChild(text);
    document.body.appendChild(node);
    const mockTimer = new MockTimer();
    mockTimer.install();

    nodeStore.estimateWordsSeenWithDelay();
    assertEquals(0, readingMode.wordsSeen);

    mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS);
    mockTimer.uninstall();
    assertEquals(4, readingMode.wordsSeen);
  });

  test(
      'estimateWordsSeenWithDelay multiple times before delay, only counts once',
      () => {
        const node = document.createElement('p');
        const text1 =
            document.createTextNode('Would you tell us that you\'re okay?');
        const text2 = document.createTextNode('There\'s a sound at the window');
        nodeStore.setDomNode(node, 1);
        nodeStore.setDomNode(text1, 2);
        nodeStore.setDomNode(text2, 3);
        node.appendChild(text1);
        document.body.appendChild(node);
        const mockTimer = new MockTimer();
        mockTimer.install();

        // First request, with text1 in view.
        nodeStore.estimateWordsSeenWithDelay();

        // After a time less than the full delay, request again with only text2
        // in view, and no words should be counted as seen yet.
        node.removeChild(text1);
        node.appendChild(text2);
        mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS / 2);
        nodeStore.estimateWordsSeenWithDelay();
        assertEquals(0, readingMode.wordsSeen);

        // The above request should have reset the timer, so still no words
        // seen.
        mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS / 2);
        assertEquals(0, readingMode.wordsSeen);

        // After the full timer, we should only count the latest text shown.
        mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS / 2);
        assertEquals(6, readingMode.wordsSeen);
        mockTimer.uninstall();
      });

  test(
      'estimateWordsSeenWithDelay multiple times after delay, accumulates count',
      () => {
        const node = document.createElement('p');
        const text1 = document.createTextNode('that he struck you');
        const text2 = document.createTextNode('a crescendo, Annie');
        nodeStore.setDomNode(node, 1);
        nodeStore.setDomNode(text1, 2);
        nodeStore.setDomNode(text2, 3);
        node.appendChild(text1);
        document.body.appendChild(node);
        const mockTimer = new MockTimer();
        mockTimer.install();

        // First request, with text1 in view.
        nodeStore.estimateWordsSeenWithDelay();
        mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS);
        assertEquals(4, readingMode.wordsSeen);

        // Second request, with only text2 in view, we should count all text
        // as being read since there was the full delay between each.
        node.removeChild(text1);
        node.appendChild(text2);
        nodeStore.estimateWordsSeenWithDelay();
        mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS);
        assertEquals(7, readingMode.wordsSeen);
        mockTimer.uninstall();
      });

  test('estimateWordsSeenWithDelay does not double count the same text', () => {
    const node = document.createElement('p');
    const text1 = document.createTextNode('He came into your apartment');
    const text2 =
        document.createTextNode('and left the bloodstains on the carpet');
    nodeStore.setDomNode(node, 1);
    nodeStore.setDomNode(text1, 2);
    nodeStore.setDomNode(text2, 3);
    node.appendChild(text1);
    document.body.appendChild(node);
    const mockTimer = new MockTimer();
    mockTimer.install();

    // First request, with text1 in view.
    nodeStore.estimateWordsSeenWithDelay();
    mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS);
    assertEquals(5, readingMode.wordsSeen);

    // Second request, with both text1 and text2 in view, we should not count
    // text1 again.
    node.appendChild(text2);
    nodeStore.estimateWordsSeenWithDelay();
    mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS);
    assertEquals(12, readingMode.wordsSeen);
    mockTimer.uninstall();
  });

  test('estimateWordsSeenWithDelay after clear resets words seen', () => {
    const node = document.createElement('p');
    const text1 = document.createTextNode('You ran into the bedroom');
    const text2 = document.createTextNode('You were struck down');
    nodeStore.setDomNode(node, 1);
    nodeStore.setDomNode(text1, 2);
    node.appendChild(text1);
    document.body.appendChild(node);
    const mockTimer = new MockTimer();
    mockTimer.install();

    // First request, with text1 in view.
    nodeStore.estimateWordsSeenWithDelay();
    mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS);
    assertEquals(5, readingMode.wordsSeen);

    // Simulate the nodes clearing for a new tree.
    node.removeChild(text1);
    node.appendChild(text2);
    nodeStore.clearDomNodes();
    nodeStore.setDomNode(node, 1);
    nodeStore.setDomNode(text2, 3);

    // Count only the newly visible text2.
    nodeStore.estimateWordsSeenWithDelay();
    mockTimer.tick(COUNT_WORDS_SEEN_DELAY_MS);
    assertEquals(4, readingMode.wordsSeen);
    mockTimer.uninstall();
  });
});
