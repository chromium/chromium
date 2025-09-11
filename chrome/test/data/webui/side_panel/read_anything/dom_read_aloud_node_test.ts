// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {DomReadAloudNode, ReadAloudNode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('DomReadAloudNode', () => {
  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chrome.readingMode.onConnected = () => {};
  });

  function ancestorNotEquals(
      node: DomReadAloudNode, nodesToCompare: DomReadAloudNode[]) {
    assertNotEquals(undefined, node.getBlockAncestor());
    nodesToCompare.forEach(compare => {
      assertNotEquals(undefined, compare.getBlockAncestor());
      assertNotEquals(compare.getBlockAncestor(), node.getBlockAncestor());
    });
  }
  function ancestorSame(nodesToCompare: DomReadAloudNode[]) {
    const ancestors = nodesToCompare.map(node => node.getBlockAncestor());
    const uniqueAncestors = new Set(ancestors);
    assertEquals(1, uniqueAncestors.size);
    assertNotEquals(undefined, ancestors[0]);
  }

  test('getBlockAncestor', () => {
    const header = document.createElement('h1');
    header.textContent = 'Alas';

    const paragraph = document.createElement('p');
    const bold = document.createElement('strong');
    bold.textContent = 'cerca';

    const textBeforeBold = document.createTextNode('Estoy ');
    const textAfterBold = document.createTextNode(' de alancanzar mi cielo');
    paragraph.appendChild(textBeforeBold);
    paragraph.appendChild(bold);
    paragraph.appendChild(textAfterBold);

    document.body.appendChild(header);
    document.body.appendChild(paragraph);

    const headerNode = ReadAloudNode.create(header);
    const textBeforeNode = ReadAloudNode.create(textBeforeBold);
    const boldNode = ReadAloudNode.create(bold);
    const textAfterNode = ReadAloudNode.create(textAfterBold);

    // All of the nodes should be DomReadAloudNode because this test
    // should only be run when the TsSegmentation flag is true.
    assertTrue(headerNode instanceof DomReadAloudNode);
    assertTrue(textBeforeNode instanceof DomReadAloudNode);
    assertTrue(textAfterNode instanceof DomReadAloudNode);
    assertTrue(boldNode instanceof DomReadAloudNode);

    // The header should not have the same block ancestor as the paragraph
    // nodes.
    ancestorNotEquals(headerNode, [textBeforeNode, textAfterNode, boldNode]);

    // All of the nodes in the paragraph element should have the same
    // block ancestor.
    ancestorSame([textBeforeNode, boldNode, textAfterNode]);
  });
});
