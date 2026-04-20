// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRectIndexAtY, getRectsForSegments, getTextNodeOffsets, ReadAloudNode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

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

  suite('getRectsForSegments', () => {
    function assertRectsSorted(rects: DOMRect[]) {
      for (let i = 0; i < rects.length - 1; i++) {
        assertTrue(rects[i]!.bottom <= rects[i + 1]!.bottom);
      }
    }

    test('empty segments returns empty array', () => {
      assertEquals(0, getRectsForSegments([]).length);
    });

    test('single valid node returns rects', () => {
      const div = document.createElement('div');
      div.textContent = 'Clocks tick and phones still ring';
      document.body.appendChild(div);

      const segments =
          [{node: ReadAloudNode.create(div)!, start: 0, length: 6}];
      const rects = getRectsForSegments(segments);

      assertTrue(rects.length > 0);
    });

    test('segment in container with multiple text nodes returns rects', () => {
      const root = document.createElement('div');
      const child1 = document.createTextNode('The world ');
      const child2 = document.createElement('span');
      const child2Text = document.createTextNode('carries on');
      const child3 = document.createTextNode(' like mad');
      child2.appendChild(child2Text);
      root.appendChild(child1);
      root.appendChild(child2);
      root.appendChild(child3);
      document.body.appendChild(root);

      const segments =
          [{node: ReadAloudNode.create(root)!, start: 11, length: 5}];
      const rects = getRectsForSegments(segments);

      assertTrue(rects.length > 0);
    });

    test('multiple segments for same node return combined rects', () => {
      const div = document.createElement('div');
      div.textContent = 'But nobody sees a thing';
      document.body.appendChild(div);

      const segments = [
        {node: ReadAloudNode.create(div)!, start: 0, length: 3},
        {node: ReadAloudNode.create(div)!, start: 4, length: 6},
      ];
      const rects = getRectsForSegments(segments);

      assertTrue(rects.length > 0);
      assertRectsSorted(rects);
    });

    test('overlapping segments return combined rects', () => {
      const div = document.createElement('div');
      div.textContent = 'Whispering behind their hands';
      document.body.appendChild(div);

      const segments = [
        {node: ReadAloudNode.create(div)!, start: 0, length: 10},
        {node: ReadAloudNode.create(div)!, start: 3, length: 7},
      ];
      const rects = getRectsForSegments(segments);

      assertTrue(rects.length > 0);
      assertRectsSorted(rects);
    });

    test('non-contiguous segments return combined rects', () => {
      const div1 = document.createElement('div');
      div1.textContent = 'Lost';
      const div2 = document.createElement('div');
      div2.textContent = 'for';
      document.body.appendChild(div1);
      document.body.appendChild(div2);

      const segments = [
        {node: ReadAloudNode.create(div1)!, start: 0, length: 4},
        {node: ReadAloudNode.create(div2)!, start: 0, length: 3},
      ];
      const rects = getRectsForSegments(segments);

      assertTrue(rects.length > 0);
      assertRectsSorted(rects);
    });
  });

  suite('getRectIndexAtY', () => {
    const rect1 = {bottom: 30} as DOMRect;
    const rect2 = {bottom: 60} as DOMRect;
    const rects = [rect1, rect2];

    test('Y matches first rect returns 0', () => {
      assertEquals(0, getRectIndexAtY(15, rects, true));
      assertEquals(0, getRectIndexAtY(15, rects, false));
    });

    test('Y matches second rect returns 1', () => {
      assertEquals(1, getRectIndexAtY(45, rects, true));
      assertEquals(0, getRectIndexAtY(45, rects, false));
    });

    test('Y in between rects with isForward=true returns current', () => {
      assertEquals(1, getRectIndexAtY(35, rects, true));
    });

    test('Y in between rects with isForward=false returns previous', () => {
      assertEquals(0, getRectIndexAtY(35, rects, false));
    });

    test('Y before all rects returns first index', () => {
      assertEquals(0, getRectIndexAtY(0, rects, true));
      assertEquals(0, getRectIndexAtY(0, rects, false));
    });

    test('Y past all rects returns last index', () => {
      assertEquals(1, getRectIndexAtY(65, rects, true));
      assertEquals(1, getRectIndexAtY(65, rects, false));
    });
  });
});
