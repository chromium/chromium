// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {currentReadHighlightClass, previousReadHighlightClass} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {FakeTree} from './fake_tree_builder.js';
import {FakeTreeBuilder} from './fake_tree_builder.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

// Tests that user selections interact well with read aloud highlights. The
// latter changes how the tree looks in reading mode, so make sure selecting in
// reading mode or in the main panel doesn't get affected.
//
// "selection" means user selection of text in either reading mode or the main
// panel. anchor node is the node the selection starts in and anchor offset is
// the index within that node that the selection starts at. the same goes for
// focus node and offset except it applies to where the selection ends.
//
// "highlight" means the reading mode highlight that indicates what text
// has been or is being read out loud.
suite('UpdateContentSelectionWithHighlights', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let fakeTree: FakeTree;

  const textNodeIds = [3, 5, 7, 9];
  const texts = [
    'From the day we arrive on the planet',
    'And blinking, step into the sun',
    'There\'s more to see than can ever be seen, more to do than can ever be done',
    'In the circle of life',
  ];

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);

    assertEquals(textNodeIds.length, 4);
    assertEquals(texts.length, 4);
    fakeTree = new FakeTreeBuilder()
                   .root(1)
                   .addTag(2, /* parentId= */ 1, 'p')
                   .addText(textNodeIds[0]!!, /* parentId= */ 2, texts[0]!)
                   .addTag(4, /* parentId= */ 1, 'p')
                   .addText(textNodeIds[1]!, /* parentId= */ 4, texts[1]!)
                   .addTag(6, /* parentId= */ 1, 'p')
                   .addText(textNodeIds[2]!, /* parentId= */ 6, texts[2]!)
                   .addTag(8, /* parentId= */ 1, 'p')
                   .addText(textNodeIds[3]!, /* parentId= */ 8, texts[3]!)
                   .build(readingMode);
    app.updateContent();
  });

  function markHighlightedNodesPrevious() {
    // Bypass Typescript compiler to allow us to get a private property
    // @ts-ignore
    app.resetPreviousHighlight();
  }

  function highlightNode(id: number) {
    // highlight the previous nodes
    let i = 0;
    while (textNodeIds[i]! !== id) {
      fakeTree.highlightNode(textNodeIds[i]!);
      app.highlightNodes([textNodeIds[i]!]);
      i++;
    }
    markHighlightedNodesPrevious();

    // highlight given node
    fakeTree.highlightNode(id);
    app.highlightNodes([id]);
  }

  function setReadingHighlight(
      fromId: number, fromOffset: number, toId: number, toOffset: number) {
    // highlight the previous nodes
    let i = 0;
    while (fromId !== textNodeIds[i]!) {
      fakeTree.highlightNode(textNodeIds[i]!);
      app.highlightNodes([textNodeIds[i]!]);
      i++;
    }
    markHighlightedNodesPrevious();

    // highlight given nodes
    fakeTree.setReadingHighlight(fromId, fromOffset, toId, toOffset);
    const nodeIds = [fromId];
    if (toId !== fromId) {
      nodeIds.push(toId);
    }
    app.highlightNodes(nodeIds);
  }

  suite('main panel selection is correct when selecting in the app: ', () => {
    const previousSelector = '.' + previousReadHighlightClass;
    const currentSelector = '.' + currentReadHighlightClass;
    const afterSelector = 'p';

    let selection: Selection;
    let actualAnchorId: number;
    let actualFocusId: number;
    let actualAnchorOffset: number;
    let actualFocusOffset: number;

    // Looks for the node containing the given text inside the given selectors
    function getTextNode(selector: string, text: string): Node {
      const nodesToCheck =
          Array.from(app.shadowRoot!.querySelectorAll(selector));
      const parentNodeWithText =
          nodesToCheck.find((element) => element.textContent!.includes(text));
      return parentNodeWithText!.firstChild!;
    }

    // Sets the reading mode selection
    async function selectNodes(
        selector: string, anchorOffset: number, fullAnchorText: string,
        focusOffset: number, fullFocusText?: string): Promise<void> {
      const anchorText = fullAnchorText.slice(anchorOffset);
      const anchorNode = getTextNode(selector, anchorText);
      const focusText =
          fullFocusText ? fullFocusText.slice(0, focusOffset) : anchorText;
      const focusNode =
          fullFocusText ? getTextNode(selector, focusText) : anchorNode;
      selection.setBaseAndExtent(
          anchorNode, anchorOffset, focusNode, focusOffset);
      return flushTasks();
    }

    setup(() => {
      selection = app.getSelection();
      actualAnchorId = -1;
      actualFocusId = -1;
      actualAnchorOffset = -1;
      actualFocusOffset = -1;

      // Capture what's sent off to the main panel so we can verify
      chrome.readingMode.onSelectionChange =
          (anchorId, anchorOffset, focusId, focusOffset) => {
            actualAnchorId = anchorId;
            actualAnchorOffset = anchorOffset;
            actualFocusId = focusId;
            actualFocusOffset = focusOffset;
          };
    });

    test('one node selected before single node highlight', async () => {
      highlightNode(textNodeIds[1]!);

      // select a subset of previous node
      const expectedAnchorOffset = 0;
      const expectedFocusOffset = 5;
      const expectedNodeId = textNodeIds[0]!;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[0]!,
          expectedFocusOffset);

      assertEquals(actualAnchorId, expectedNodeId);
      assertEquals(actualFocusId, expectedNodeId);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test('multiple nodes selected before single node highlight', async () => {
      highlightNode(textNodeIds[2]!);

      // select a subset of previous nodes
      const expectedAnchorOffset = 1;
      const expectedFocusOffset = 7;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[0]!,
          expectedFocusOffset, texts[1]!);

      assertEquals(actualAnchorId, textNodeIds[0]!);
      assertEquals(actualFocusId, textNodeIds[1]!);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test('one node selected before multiple node highlight', async () => {
      // highlight last two nodes
      setReadingHighlight(
          textNodeIds[2]!, 0, textNodeIds[3]!, texts[3]!.length);

      // select a subset of one previous node
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      const expectedNodeId = textNodeIds[1]!;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[1]!,
          expectedFocusOffset);

      assertEquals(actualAnchorId, expectedNodeId);
      assertEquals(actualFocusId, expectedNodeId);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test('multiple nodes selected before multiple node highlight', async () => {
      // highlight last two nodes
      setReadingHighlight(
          textNodeIds[2]!, 0, textNodeIds[3]!, texts[3]!.length);

      // select a subset of previous nodes
      const expectedAnchorOffset = 1;
      const expectedFocusOffset = 7;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[0]!,
          expectedFocusOffset, texts[1]!);

      assertEquals(actualAnchorId, textNodeIds[0]!);
      assertEquals(actualFocusId, textNodeIds[1]!);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test('one node selected inside single node highlight', async () => {
      const highlightId = textNodeIds[1]!;
      highlightNode(highlightId);

      // select a subset of the highlighted node
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      await selectNodes(
          currentSelector, expectedAnchorOffset, texts[1]!,
          expectedFocusOffset);

      assertEquals(actualAnchorId, highlightId);
      assertEquals(actualFocusId, highlightId);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test('one node selected inside multiple node highlight', async () => {
      // highlight two nodes
      setReadingHighlight(
          textNodeIds[0]!, 0, textNodeIds[1]!, texts[1]!.length);

      // select a subset of one of the highlighted nodes
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      await selectNodes(
          currentSelector, expectedAnchorOffset, texts[1]!,
          expectedFocusOffset);

      assertEquals(actualAnchorId, textNodeIds[1]!);
      assertEquals(actualFocusId, textNodeIds[1]!);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test('prefix selected in long reading highlight', async () => {
      const highlightId = textNodeIds[2]!;
      const highlightStart = 11;
      setReadingHighlight(
          highlightId, highlightStart, highlightId, texts[2]!.length);

      // select node up to where it's highlighted
      const expectedAnchorOffset = 0;
      await selectNodes(
          previousSelector, expectedAnchorOffset,
          texts[2]!.slice(0, highlightStart), highlightStart);

      assertEquals(actualAnchorId, highlightId);
      assertEquals(actualFocusId, highlightId);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, highlightStart);
    });

    test('suffix selected in long reading highlight', async () => {
      const highlightId = textNodeIds[2]!;
      const highlightEnd = 15;
      setReadingHighlight(highlightId, 0, highlightId, highlightEnd);

      // select node starting after the end of the highlight
      const selectedText = texts[2]!.slice(highlightEnd);
      const nodesToCheck = Array.from(app.shadowRoot!.querySelectorAll('span'));
      const parentNodeWithText = nodesToCheck.find(
          (element) => element.textContent!.includes(selectedText));
      const textNode = parentNodeWithText!.lastChild!;
      selection.setBaseAndExtent(textNode, 0, textNode, selectedText.length);
      await flushTasks();

      assertEquals(actualAnchorId, highlightId);
      assertEquals(actualFocusId, highlightId);
      assertEquals(actualAnchorOffset, highlightEnd);
      assertEquals(actualFocusOffset, texts[2]!.length);
    });

    test('one node selected after reading highlight', async () => {
      highlightNode(textNodeIds[1]!);

      // select a subset of node after highlight
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 8;
      await selectNodes(
          afterSelector, expectedAnchorOffset, texts[2]!, expectedFocusOffset);

      assertEquals(actualAnchorId, textNodeIds[2]!);
      assertEquals(actualFocusId, textNodeIds[2]!);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test('multiple nodes selected after reading highlight', async () => {
      highlightNode(textNodeIds[0]!);

      // select a subset of nodes after highlight
      const expectedAnchorOffset = 4;
      const expectedFocusOffset = 10;
      await selectNodes(
          afterSelector, expectedAnchorOffset, texts[1]!, expectedFocusOffset,
          texts[2]!);

      assertEquals(actualAnchorId, textNodeIds[1]!);
      assertEquals(actualFocusId, textNodeIds[2]!);
      assertEquals(actualAnchorOffset, expectedAnchorOffset);
      assertEquals(actualFocusOffset, expectedFocusOffset);
    });

    test(
        'selection across previous, current, and after highlight', async () => {
          // highlight middle of node 7
          const highlightStart = 15;
          const highlightEnd = 25;
          setReadingHighlight(7, highlightStart, 7, highlightEnd);

          // select all text
          const expectedAnchorOffset = 0;
          const expectedFocusOffset = texts[3]!.length;
          const anchorNode = getTextNode(previousSelector, texts[0]!);
          const focusText = texts[3]!;
          const focusNode = getTextNode('p', focusText);
          selection.setBaseAndExtent(
              anchorNode, 0, focusNode, focusText.length);
          await flushTasks();

          assertEquals(actualAnchorId, textNodeIds[0]!);
          assertEquals(actualFocusId, textNodeIds[3]!);
          assertEquals(actualAnchorOffset, expectedAnchorOffset);
          assertEquals(actualFocusOffset, expectedFocusOffset);
        });
  });

  suite('app selection is correct when selecting from the main panel: ', () => {
    setup(() => {
      document.onselectionchange = () => {};
    });

    test('one node selected before single node highlight', () => {
      highlightNode(5);

      // select a subset of node 3
      const expectedAnchorOffset = 0;
      const expectedFocusOffset = 5;
      fakeTree.setSelection(3, expectedAnchorOffset, 3, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[0]!);
      assertEquals(selection.focusNode.textContent, texts[0]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test('multiple nodes selected before single node highlight', () => {
      highlightNode(7);

      // select a subset of nodes 3 and 5
      const expectedAnchorOffset = 1;
      const expectedFocusOffset = 7;
      fakeTree.setSelection(3, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[0]!);
      assertEquals(selection.focusNode.textContent, texts[1]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test('one node selected before multiple node highlight', () => {
      setReadingHighlight(7, 0, 9, texts[3]!.length);

      // select a subset of node 5
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 6;
      fakeTree.setSelection(5, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[1]!);
      assertEquals(selection.focusNode.textContent, texts[1]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test('multiple nodes selected before multiple node highlight', () => {
      setReadingHighlight(7, 0, 9, texts[3]!.length);

      // select a subset of nodes 3 and 5
      const expectedAnchorOffset = 6;
      const expectedFocusOffset = 2;
      fakeTree.setSelection(3, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[0]!);
      assertEquals(selection.focusNode.textContent, texts[1]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test('one node selected inside single node highlight', () => {
      const highlightId = 5;
      highlightNode(highlightId);

      // select a subset of node 5
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      fakeTree.setSelection(
          highlightId, expectedAnchorOffset, highlightId, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[1]!);
      assertEquals(selection.focusNode.textContent, texts[1]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test('one node selected inside multiple node highlight', () => {
      setReadingHighlight(3, 0, 5, texts[2]!.length);

      // select a subset of node 5
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      fakeTree.setSelection(5, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[1]!);
      assertEquals(selection.focusNode.textContent, texts[1]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test(
        'multiple nodes selected inside the multiple node reading highlight',
        () => {
          setReadingHighlight(3, 0, 5, texts[2]!.length);

          // select a subset of nodes 3 and 5
          const expectedAnchorOffset = 4;
          const expectedFocusOffset = 9;
          fakeTree.setSelection(
              3, expectedAnchorOffset, 5, expectedFocusOffset);
          app.updateSelection();

          const selection = app.getSelection();
          assertEquals(selection.anchorNode.textContent, texts[0]!);
          assertEquals(selection.focusNode.textContent, texts[1]!);
          assertEquals(selection.anchorOffset, expectedAnchorOffset);
          assertEquals(selection.focusOffset, expectedFocusOffset);
        });

    test('prefix selected in long reading highlight', () => {
      const highlightId = 7;
      const highlightStart = 5;
      setReadingHighlight(
          highlightId, highlightStart, highlightId, texts[2]!.length);

      // select node 7 up to where it's highlighted
      const expectedAnchorOffset = 0;
      fakeTree.setSelection(
          highlightId, expectedAnchorOffset, highlightId, highlightStart);
      app.updateSelection();

      const selection = app.getSelection();
      const expectedSelectedText = texts[2]!.slice(0, highlightStart);
      assertEquals(selection.anchorNode.textContent, expectedSelectedText);
      assertEquals(selection.focusNode.textContent, expectedSelectedText);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, highlightStart);
    });

    test('suffix selected in long reading highlight', () => {
      const highlightId = 7;
      const highlightEnd = 5;
      setReadingHighlight(highlightId, 0, highlightId, highlightEnd);

      // select node 7 starting after the end of the highlight
      fakeTree.setSelection(
          highlightId, highlightEnd, highlightId, texts[2]!.length);
      app.updateSelection();

      // The highlight should have split node 7 into two nodes - one with the
      // text that's highlighted and another with the remaining text. Since we
      // selected the rest of node 7, the selection corresponds to the
      // entirety of the new second node.
      const selection = app.getSelection();
      const expectedSelectedText = texts[2]!.slice(highlightEnd);
      assertEquals(selection.anchorNode.textContent, expectedSelectedText);
      assertEquals(selection.focusNode.textContent, expectedSelectedText);
      assertEquals(selection.anchorOffset, 0);
      assertEquals(selection.focusOffset, expectedSelectedText.length);
    });

    test('one node selected after reading highlight', () => {
      highlightNode(5);

      // select a subset of node 7
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 8;
      fakeTree.setSelection(7, expectedAnchorOffset, 7, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[2]!);
      assertEquals(selection.focusNode.textContent, texts[2]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test('multiple nodes selected after reading highlight', () => {
      highlightNode(3);

      // select a subset of nodes 5 and 7
      const expectedAnchorOffset = 4;
      const expectedFocusOffset = 10;
      fakeTree.setSelection(5, expectedAnchorOffset, 7, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorNode.textContent, texts[1]!);
      assertEquals(selection.focusNode.textContent, texts[2]!);
      assertEquals(selection.anchorOffset, expectedAnchorOffset);
      assertEquals(selection.focusOffset, expectedFocusOffset);
    });

    test('selection across previous, current, and after highlight', () => {
      // highlight middle of node 7
      const highlightStart = 15;
      const highlightEnd = 25;
      setReadingHighlight(7, highlightStart, 7, highlightEnd);

      // select all text
      fakeTree.setSelection(3, 0, 9, texts[3]!.length);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(selection.anchorOffset, 0);
      assertEquals(selection.focusOffset, texts[3]!.length);
      assertEquals(selection.anchorNode.textContent, texts[0]!);
      assertEquals(selection.focusNode.textContent, texts[3]!);
    });
  });
});
