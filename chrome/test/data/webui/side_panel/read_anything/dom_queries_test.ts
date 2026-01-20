// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTextNodeOffsets} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('DomQueries', () => {
  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test(
      'getTextNodeOffsets returns correct text node and offset at node boundaries',
      () => {
        const root = document.createElement('div');
        const child1 = document.createTextNode('Where it began');
        const child2 = document.createElement('span');
        const child2Text =
            document.createTextNode('I can\'t begin to know when');
        const child3 =
            document.createTextNode('But then I know it\'s growing strong');
        child2.appendChild(child2Text);
        root.appendChild(child1);
        root.appendChild(child2);
        root.appendChild(child3);
        document.body.appendChild(root);

        let start = 0;
        let result = getTextNodeOffsets(root, start);
        assertEquals(child1.textContent, result.node.textContent);
        assertEquals(0, result.offset);

        start += child1.textContent.length;
        result = getTextNodeOffsets(root, start);
        assertEquals(child2.textContent, result.node.textContent);
        assertEquals(start, result.offset);

        start += child2.textContent.length;
        result = getTextNodeOffsets(root, start);
        assertEquals(child3.textContent, result.node.textContent);
        assertEquals(start, result.offset);
      });

  test(
      'getTextNodeOffsets returns correct text node and offset between node boundaries',
      () => {
        const root = document.createElement('div');
        const child1 = document.createTextNode('Was in the spring');
        const child2 = document.createElement('span');
        const child2Text =
            document.createTextNode('And spring became the summer');
        const child3 =
            document.createTextNode('Who\'d have believe you\'d come along?\n');
        child2.appendChild(child2Text);
        root.appendChild(child1);
        root.appendChild(child2);
        root.appendChild(child3);
        document.body.appendChild(root);

        let start = 5;
        let result = getTextNodeOffsets(root, start);
        assertEquals(child1.textContent, result.node.textContent);
        assertEquals(0, result.offset);

        start += child1.textContent.length;
        result = getTextNodeOffsets(root, start);
        assertEquals(child2.textContent, result.node.textContent);
        assertEquals(child1.textContent.length, result.offset);

        start += child2.textContent.length;
        result = getTextNodeOffsets(root, start);
        assertEquals(child3.textContent, result.node.textContent);
        assertEquals(
            child1.textContent.length + child2.textContent.length,
            result.offset);
      });

  test(
      'getTextNodeOffsets returns root node and end for out of bounds start',
      () => {
        const root = document.createElement('div');
        const child = document.createTextNode('Hands');
        root.appendChild(child);
        document.body.appendChild(root);
        const start = child.textContent.length + 10;

        const result = getTextNodeOffsets(root, start);

        assertEquals(root, result.node);
        assertEquals(child.textContent.length, result.offset);
      });

  test(
      'getTextNodeOffsets returns same node and no offset for text node',
      () => {
        const textNode = document.createTextNode('Touchin\' hands');
        document.body.appendChild(textNode);
        const start = 5;

        const result = getTextNodeOffsets(textNode, start);

        assertEquals(textNode, result.node);
        assertEquals(0, result.offset);
      });
});
