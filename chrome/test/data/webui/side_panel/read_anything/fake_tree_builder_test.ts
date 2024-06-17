// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {assertEquals, assertThrows, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import type {FakeTree} from './fake_tree_builder.js';
import {FakeTreeBuilder} from './fake_tree_builder.js';

suite('FakeTreeBuilderTest', () => {
  let readingMode: FakeReadingMode;
  const errorMsg: string = ': expected false to be true';

  setup(() => {
    readingMode = new FakeReadingMode();
  });

  function buildTree(
      builder: FakeTreeBuilder, rootId: number, htmlTagNodes: number[],
      htmlTags: string[], textNodes: number[], text: string[]): FakeTree {
    builder.root(rootId);
    for (let i = 0; i < htmlTagNodes.length; i++) {
      builder.addTag(htmlTagNodes[i]!, rootId, htmlTags[i]!)
          .addText(textNodes[i]!, htmlTagNodes[i]!, text[i]!);
    }
    return builder.build(readingMode);
  }

  suite('fake tree builder', () => {
    let builder: FakeTreeBuilder;

    function assertNodeHasExpectedValues(
        tree: FakeTree, expectedId: number, expectedHtmlTag: string,
        expectedTextContent: string, expectedChildren: number[]) {
      assertTrue(tree.nodes.has(expectedId));
      const node = tree.nodes.get(expectedId)!;
      assertEquals(node.id, expectedId);

      assertEquals(expectedHtmlTag, node.htmlTag);
      assertEquals(expectedHtmlTag, readingMode.getHtmlTag(expectedId));

      assertEquals(expectedTextContent, node.textContent);
      assertEquals(expectedTextContent, readingMode.getTextContent(expectedId));

      const readingModeChildren = readingMode.getChildren(expectedId);
      assertEquals(expectedChildren.length, readingModeChildren.length);
      assertEquals(expectedChildren.length, node.children.length);
      for (let i = 0; i < expectedChildren.length; i++) {
        assertEquals(expectedChildren[i], readingModeChildren[i]!);
        assertEquals(expectedChildren[i], node.children[i]!);
      }
    }

    setup(() => {
      builder = new FakeTreeBuilder();
    });

    test('build without root node fails', () => {
      assertThrows(() => builder.build(readingMode), errorMsg);
    });

    test('add tag before parent fails', () => {
      assertThrows(() => builder.addTag(1, 0, 'span'), errorMsg);
    });

    test('add text before parent fails', () => {
      assertThrows(
          () => builder.addText(2, 1, 'just give me a reason'), errorMsg);
    });

    test('build tree with just root returns root with no children', () => {
      const rootId = 0;
      const tree = builder.root(rootId).build(readingMode);

      assertEquals(rootId, readingMode.rootId);
      assertNodeHasExpectedValues(tree, rootId, '#document', '', []);
    });

    suite('with valid tree', () => {
      const rootId = 10;
      const htmlTagNodes = [11, 13, 15, 17];
      const htmlTags = ['h1', 'p', 'h2', 'span'];
      const textNodes = [12, 14, 16, 18];
      const text = [
        'Verse 1',
        'right from the start you were a thief you stole my heart',
        'Chorus',
        'just give me a reason, just a little bit\'s enough',
      ];

      let tree: FakeTree;

      setup(() => {
        tree =
            buildTree(builder, rootId, htmlTagNodes, htmlTags, textNodes, text);
      });

      test('root of tree has document tag and html tag children', () => {
        assertEquals(rootId, readingMode.rootId);
        assertNodeHasExpectedValues(
            tree, rootId, '#document', '', htmlTagNodes);
      });

      test(
          'tags have html tags, no text content, and text nodes as children',
          () => {
            for (let i = 0; i < htmlTagNodes.length; i++) {
              assertNodeHasExpectedValues(
                  tree, htmlTagNodes[i]!, htmlTags[i]!, '', [textNodes[i]!]);
            }
          });

      test('text nodes have text content, no tags, and no children', () => {
        for (let i = 0; i < textNodes.length; i++) {
          assertNodeHasExpectedValues(tree, textNodes[i]!, '', text[i]!, []);
        }
      });
    });
  });

  suite('fake tree', () => {
    const rootId = 10;
    const htmlTagNodes = [11, 13, 15, 17, 19];
    const htmlTags = ['p', 'p', 'p', 'p', 'p'];
    const textNodes = [12, 14, 16, 18, 20];
    const text = [
      'Maria',
      'I\'ve just met a girl named Maria',
      'And suddenly that name',
      'Will never be the same',
      'To me',
    ];

    let tree: FakeTree;

    setup(() => {
      const builder = new FakeTreeBuilder();
      tree =
          buildTree(builder, rootId, htmlTagNodes, htmlTags, textNodes, text);
    });

    suite('setSelection', () => {
      test('non-existent nodes fails', () => {
        assertThrows(() => tree.setSelection(4, 0, 5, 7), errorMsg);
      });

      test('selection within one node succeeds', () => {
        tree.setSelection(14, 3, 14, 10);

        assertEquals(14, readingMode.startNodeId);
        assertEquals(14, readingMode.endNodeId);
        assertEquals(3, readingMode.startOffset);
        assertEquals(10, readingMode.endOffset);
      });

      test('selection across nodes succeeds', () => {
        tree.setSelection(14, 0, 16, 5);

        assertEquals(14, readingMode.startNodeId);
        assertEquals(16, readingMode.endNodeId);
        assertEquals(0, readingMode.startOffset);
        assertEquals(5, readingMode.endOffset);
      });
    });

    suite('setReadingHighlight', () => {
      test('non-existent nodes fails', () => {
        assertThrows(() => tree.setReadingHighlight(2, 1, 3, 2), errorMsg);
      });

      suite('when highlighting one node', () => {
        setup(() => {
          tree.setReadingHighlight(textNodes[1]!, 3, textNodes[1]!, 10);
        });

        test('indices for highlighted node are correct', () => {
          assertEquals(3, readingMode.getCurrentTextStartIndex(textNodes[1]!));
          assertEquals(10, readingMode.getCurrentTextEndIndex(textNodes[1]!));
        });

        test('indices for valid but not highlighted node are not found', () => {
          assertEquals(-1, readingMode.getCurrentTextStartIndex(textNodes[2]!));
          assertEquals(-1, readingMode.getCurrentTextEndIndex(textNodes[2]!));
        });

        test('indices for invalid node are not found', () => {
          assertEquals(-1, readingMode.getCurrentTextStartIndex(0));
          assertEquals(-1, readingMode.getCurrentTextEndIndex(0));
        });
      });

      suite('when highlighting across nodes', () => {
        setup(() => {
          tree.setReadingHighlight(textNodes[1]!, 4, textNodes[3]!, 5);
        });

        test('indices for first highlighted node are correct', () => {
          assertEquals(4, readingMode.getCurrentTextStartIndex(textNodes[1]!));
          assertEquals(
              readingMode.getCurrentTextEndIndex(textNodes[1]!),
              text[1]!.length);
        });

        test('indices for last highlighted node are correct', () => {
          assertEquals(0, readingMode.getCurrentTextStartIndex(textNodes[3]!));
          assertEquals(5, readingMode.getCurrentTextEndIndex(textNodes[3]!));
        });

        test('indices for invalid node are not found', () => {
          assertEquals(-1, readingMode.getCurrentTextStartIndex(0));
          assertEquals(-1, readingMode.getCurrentTextEndIndex(0));
        });
      });
    });

    suite('highlightNode', () => {
      test('non-existent nodes fails', () => {
        assertThrows(() => tree.highlightNode(0), errorMsg);
      });

      suite('when highlighting valid node', () => {
        setup(() => {
          tree.highlightNode(textNodes[2]!);
        });

        test('full node is highlighted', () => {
          assertEquals(0, readingMode.getCurrentTextStartIndex(textNodes[2]!));
          assertEquals(
              readingMode.getCurrentTextEndIndex(textNodes[2]!),
              text[2]!.length);
        });

        test('valid but not highlighted node has invalid indices', () => {
          assertEquals(-1, readingMode.getCurrentTextStartIndex(textNodes[3]!));
          assertEquals(-1, readingMode.getCurrentTextEndIndex(textNodes[3]!));
        });

        test('indices for invalid node are not found', () => {
          assertEquals(-1, readingMode.getCurrentTextStartIndex(0));
          assertEquals(-1, readingMode.getCurrentTextEndIndex(0));
        });
      });
    });
  });
});
