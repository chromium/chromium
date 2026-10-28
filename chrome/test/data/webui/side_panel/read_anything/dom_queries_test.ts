// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFirstTextNodeAtOrAfter, getLastTextNodeBefore, getNearestTextBoundaryPoint, getRectsForSegments, getTextNodeOffsets, ReadAloudNode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

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
      'getTextNodeOffsets returns correct node at end of text node boundary',
      () => {
        const root = document.createElement('div');
        const child = document.createTextNode('End of the line');
        root.appendChild(child);
        document.body.appendChild(root);

        // Offset exactly at the end of the text node
        const start = child.textContent.length;
        const result = getTextNodeOffsets(root, start);

        assertEquals(child, result.node);
        assertEquals(0, result.offset);
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

  suite('getNearestTextBoundaryPoint', () => {
    test('returns same node and offset if inside a text node', () => {
      const textNode = document.createTextNode('Hello world');
      const root = document.createElement('div');
      root.appendChild(textNode);
      document.body.appendChild(root);

      const result = getNearestTextBoundaryPoint(textNode, 5, root, true);
      assertEquals(textNode, result.node);
      assertEquals(5, result.offset);
    });

    test(
        'shifts start boundary forward to next text node if at end of text node',
        () => {
          const root = document.createElement('div');
          const child1 = document.createTextNode('hello');
          const child2 = document.createTextNode('world');
          root.appendChild(child1);
          root.appendChild(child2);
          document.body.appendChild(root);

          const result = getNearestTextBoundaryPoint(child1, 5, root, true);
          assertEquals(child2, result.node);
          assertEquals(0, result.offset);
        });

    test(
        'shifts end boundary backward to previous text node if at start of text node',
        () => {
          const root = document.createElement('div');
          const child1 = document.createTextNode('hello');
          const child2 = document.createTextNode('world');
          root.appendChild(child1);
          root.appendChild(child2);
          document.body.appendChild(root);

          const result = getNearestTextBoundaryPoint(child2, 0, root, false);
          assertEquals(child1, result.node);
          assertEquals(5, result.offset);
        });

    test(
        'normalizes Element node using forward search when offset is valid index',
        () => {
          const root = document.createElement('div');
          const child1 = document.createTextNode('hello');
          const child2 = document.createElement('span');
          const child2Text = document.createTextNode('world');
          child2.appendChild(child2Text);
          root.appendChild(child1);
          root.appendChild(child2);
          document.body.appendChild(root);

          const result = getNearestTextBoundaryPoint(root, 0, root, true);
          assertEquals(child1, result.node);
          assertEquals(0, result.offset);
        });

    test(
        'normalizes Element node using backward search when offset wraps out of bounds',
        () => {
          const root = document.createElement('div');
          const child1 = document.createTextNode('hello');
          const child2 = document.createElement('span');
          const child2Text = document.createTextNode('world');
          child2.appendChild(child2Text);
          root.appendChild(child1);
          root.appendChild(child2);
          document.body.appendChild(root);

          const result = getNearestTextBoundaryPoint(root, 2, root, true);
          assertEquals(child2Text, result.node);
          assertEquals(5, result.offset);
        });

    test(
        'shifts start boundary forward skipping unmapped and whitespace nodes',
        () => {
          const root = document.createElement('div');
          const child1 = document.createTextNode('hello');
          const spacer1 = document.createTextNode('   \n');
          const spacer2 = document.createTextNode(' ');
          const child2 = document.createTextNode('world');
          root.appendChild(child1);
          root.appendChild(spacer1);
          root.appendChild(spacer2);
          root.appendChild(child2);
          document.body.appendChild(root);

          const mappedNodes = new Set<Node>([child1, child2]);
          const isValidTarget = (node: Node) => mappedNodes.has(node);

          const result = getNearestTextBoundaryPoint(
              child1, 5, root, /*isStart=*/ true, isValidTarget);
          assertEquals(child2, result.node);
          assertEquals(0, result.offset);
        });

    test(
        'shifts end boundary backward skipping unmapped and whitespace nodes',
        () => {
          const root = document.createElement('div');
          const child1 = document.createTextNode('hello');
          const spacer1 = document.createTextNode('\n  ');
          const spacer2 = document.createTextNode(' ');
          const child2 = document.createTextNode('world');
          root.appendChild(child1);
          root.appendChild(spacer1);
          root.appendChild(spacer2);
          root.appendChild(child2);
          document.body.appendChild(root);

          const mappedNodes = new Set<Node>([child1, child2]);
          const isValidTarget = (node: Node) => mappedNodes.has(node);

          const result = getNearestTextBoundaryPoint(
              child2, 0, root, /*isStart=*/ false, isValidTarget);
          assertEquals(child1, result.node);
          assertEquals(5, result.offset);
        });
  });

  suite('getFirstTextNodeAtOrAfter', () => {
    let root: HTMLElement;
    let hello: Text;
    let space: Text;
    let world: Text;

    // <div>"hello"<span>" "</span>"world"</div>
    setup(() => {
      root = document.createElement('div');
      hello = document.createTextNode('hello');
      space = document.createTextNode(' ');
      world = document.createTextNode('world');
      const span = document.createElement('span');
      span.appendChild(space);
      root.append(hello, span, world);
      document.body.appendChild(root);
    });

    test('returns the first text node at or after the point', () => {
      // The point is inside the text node.
      assertEquals(hello, getFirstTextNodeAtOrAfter(hello, 2, root));
      // The point is at the end of the text node.
      assertEquals(space, getFirstTextNodeAtOrAfter(hello, 5, root));
      // The point is before the child at the offset.
      assertEquals(space, getFirstTextNodeAtOrAfter(root, 1, root));
      // The point is after all of the text.
      assertEquals(null, getFirstTextNodeAtOrAfter(root, 3, root));
    });

    test('skips text nodes that are not valid targets', () => {
      const isHello = (node: Node) => node === hello;
      const isWorld = (node: Node) => node === world;
      // The text node containing the point is valid.
      assertEquals(hello, getFirstTextNodeAtOrAfter(hello, 2, root, isHello));
      // The text node containing the point is invalid.
      assertEquals(world, getFirstTextNodeAtOrAfter(hello, 2, root, isWorld));
      // The text node in the child at the offset is invalid.
      assertEquals(world, getFirstTextNodeAtOrAfter(root, 1, root, isWorld));
      // No text node after the point is valid.
      assertEquals(null, getFirstTextNodeAtOrAfter(hello, 5, root, isHello));
    });

    test('passes each text node it considers to isValidTarget', () => {
      const considered: Node[] = [];
      const isValidTarget = (node: Node) => {
        considered.push(node);
        return node === world;
      };
      getFirstTextNodeAtOrAfter(hello, 2, root, isValidTarget);
      assertDeepEquals([hello, space, world], considered);
    });
  });

  suite('getLastTextNodeBefore', () => {
    let root: HTMLElement;
    let hello: Text;
    let space: Text;
    let world: Text;

    // <div>"hello"<span>" "</span>"world"</div>
    setup(() => {
      root = document.createElement('div');
      hello = document.createTextNode('hello');
      space = document.createTextNode(' ');
      world = document.createTextNode('world');
      const span = document.createElement('span');
      span.appendChild(space);
      root.append(hello, span, world);
      document.body.appendChild(root);
    });

    test('returns the last text node before the point', () => {
      // The point is inside the text node.
      assertEquals(world, getLastTextNodeBefore(world, 2, root));
      // The point is at the start of the text node.
      assertEquals(space, getLastTextNodeBefore(world, 0, root));
      // The point is after the child before the offset.
      assertEquals(space, getLastTextNodeBefore(root, 2, root));
      // The point is before all of the text.
      assertEquals(null, getLastTextNodeBefore(root, 0, root));
    });

    test('skips text nodes that are not valid targets', () => {
      const isHello = (node: Node) => node === hello;
      const isWorld = (node: Node) => node === world;
      // The text node containing the point is valid.
      assertEquals(world, getLastTextNodeBefore(world, 2, root, isWorld));
      // The text node containing the point is invalid.
      assertEquals(hello, getLastTextNodeBefore(world, 2, root, isHello));
      // The text node in the child before the offset is invalid.
      assertEquals(hello, getLastTextNodeBefore(root, 2, root, isHello));
      // No text node before the point is valid.
      assertEquals(null, getLastTextNodeBefore(world, 0, root, isWorld));
    });

    test('passes each text node it considers to isValidTarget', () => {
      const considered: Node[] = [];
      const isValidTarget = (node: Node) => {
        considered.push(node);
        return node === hello;
      };
      getLastTextNodeBefore(world, 2, root, isValidTarget);
      assertDeepEquals([world, space, hello], considered);
    });
  });
});
