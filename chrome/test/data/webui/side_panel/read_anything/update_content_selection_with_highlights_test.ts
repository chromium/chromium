// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {currentReadHighlightClass, previousReadHighlightClass} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

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
  let app: AppElement;
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

    assertEquals(4, textNodeIds.length);
    assertEquals(4, texts.length);
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

  async function highlightNode(id: number) {
    // highlight the previous nodes
    let i = 0;
    while (textNodeIds[i]! !== id) {
      fakeTree.highlightNode(textNodeIds[i]!);
      app.highlightCurrentGranularity([textNodeIds[i]!]);
      i++;
    }

    // highlight given node
    fakeTree.highlightNode(id);
    app.highlightCurrentGranularity([id]);
    return microtasksFinished();
  }

  async function setReadingHighlight(
      fromId: number, fromOffset: number, toId: number, toOffset: number) {
    // highlight the previous nodes
    let i = 0;
    while (fromId !== textNodeIds[i]!) {
      fakeTree.highlightNode(textNodeIds[i]!);
      app.highlightCurrentGranularity([textNodeIds[i]!]);
      i++;
    }

    // highlight given nodes
    fakeTree.setReadingHighlight(fromId, fromOffset, toId, toOffset);
    const nodeIds = [fromId];
    if (toId !== fromId) {
      nodeIds.push(toId);
    }
    app.highlightCurrentGranularity(nodeIds);
    return microtasksFinished();
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
        focusOffset: number, fullFocusText?: string) {
      const anchorText = fullAnchorText.slice(anchorOffset);
      const anchorNode = getTextNode(selector, anchorText);
      const focusText =
          fullFocusText ? fullFocusText.slice(0, focusOffset) : anchorText;
      const focusNode =
          fullFocusText ? getTextNode(selector, focusText) : anchorNode;
      selection.setBaseAndExtent(
          anchorNode, anchorOffset, focusNode, focusOffset);

      return microtasksFinished();
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

    test('with no highlight', async () => {
      const expectedAnchorOffset = 7;
      const expectedFocusOffset = 12;
      const expectedAnchorId = textNodeIds[1]!;
      const expectedFocusId = textNodeIds[3]!;

      // Select from the middle (offset 7) of the first paragraph to the middle
      // (offset 12) of the third paragraph.
      await selectNodes(
          afterSelector, expectedAnchorOffset, texts[1]!, expectedFocusOffset,
          texts[3]!);

      assertEquals(expectedAnchorId, actualAnchorId);
      assertEquals(expectedFocusId, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('one node selected before single node highlight', async () => {
      await highlightNode(textNodeIds[1]!);

      // select a subset of previous node
      const expectedAnchorOffset = 0;
      const expectedFocusOffset = 5;
      const expectedNodeId = textNodeIds[0]!;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[0]!,
          expectedFocusOffset);

      assertEquals(expectedNodeId, actualAnchorId);
      assertEquals(expectedNodeId, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('multiple nodes selected before single node highlight', async () => {
      await highlightNode(textNodeIds[2]!);
      // select a subset of previous nodes
      const expectedAnchorOffset = 1;
      const expectedFocusOffset = 7;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[0]!,
          expectedFocusOffset, texts[1]!);

      assertEquals(textNodeIds[0]!, actualAnchorId);
      assertEquals(textNodeIds[1]!, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('one node selected before multiple node highlight', async () => {
      // highlight last two nodes
      await setReadingHighlight(
          textNodeIds[2]!, 0, textNodeIds[3]!, texts[3]!.length);

      // select a subset of one previous node
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      const expectedNodeId = textNodeIds[1]!;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[1]!,
          expectedFocusOffset);

      assertEquals(expectedNodeId, actualAnchorId);
      assertEquals(expectedNodeId, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('multiple nodes selected before multiple node highlight', async () => {
      // highlight last two nodes
      await setReadingHighlight(
          textNodeIds[2]!, 0, textNodeIds[3]!, texts[3]!.length);

      // select a subset of previous nodes
      const expectedAnchorOffset = 1;
      const expectedFocusOffset = 7;
      await selectNodes(
          previousSelector, expectedAnchorOffset, texts[0]!,
          expectedFocusOffset, texts[1]!);

      assertEquals(textNodeIds[0]!, actualAnchorId);
      assertEquals(textNodeIds[1]!, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('one node selected inside single node highlight', async () => {
      const highlightId = textNodeIds[1]!;
      await highlightNode(highlightId);

      // select a subset of the highlighted node
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      await selectNodes(
          currentSelector, expectedAnchorOffset, texts[1]!,
          expectedFocusOffset);

      assertEquals(highlightId, actualAnchorId);
      assertEquals(highlightId, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('one node selected inside multiple node highlight', async () => {
      // highlight two nodes
      await setReadingHighlight(
          textNodeIds[0]!, 0, textNodeIds[1]!, texts[1]!.length);

      // select a subset of one of the highlighted nodes
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      await selectNodes(
          currentSelector, expectedAnchorOffset, texts[1]!,
          expectedFocusOffset);

      assertEquals(textNodeIds[1]!, actualAnchorId);
      assertEquals(textNodeIds[1]!, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('prefix selected in long reading highlight', async () => {
      const highlightId = textNodeIds[2]!;
      const highlightStart = 11;
      await setReadingHighlight(
          highlightId, highlightStart, highlightId, texts[2]!.length);

      // select node up to where it's highlighted
      const expectedAnchorOffset = 0;
      await selectNodes(
          previousSelector, expectedAnchorOffset,
          texts[2]!.slice(0, highlightStart), highlightStart);

      assertEquals(highlightId, actualAnchorId);
      assertEquals(highlightId, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(highlightStart, actualFocusOffset);
    });

    test('suffix selected in long reading highlight', async () => {
      const highlightId = textNodeIds[2]!;
      const highlightEnd = 15;
      await setReadingHighlight(highlightId, 0, highlightId, highlightEnd);

      // select node starting after the end of the highlight
      const selectedText = texts[2]!.slice(highlightEnd);
      const nodesToCheck = Array.from(app.shadowRoot!.querySelectorAll('span'));
      const parentNodeWithText = nodesToCheck.find(
          (element) => element.textContent!.includes(selectedText));
      const textNode = parentNodeWithText!.lastChild!;
      selection.setBaseAndExtent(textNode, 0, textNode, selectedText.length);
      await microtasksFinished();

      assertEquals(highlightId, actualAnchorId);
      assertEquals(highlightId, actualFocusId);
      assertEquals(highlightEnd, actualAnchorOffset);
      assertEquals(texts[2]!.length, actualFocusOffset);
    });

    test('one node selected after reading highlight', async () => {
      await highlightNode(textNodeIds[1]!);

      // select a subset of node after highlight
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 8;
      await selectNodes(
          afterSelector, expectedAnchorOffset, texts[2]!, expectedFocusOffset);

      assertEquals(textNodeIds[2]!, actualAnchorId);
      assertEquals(textNodeIds[2]!, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test('multiple nodes selected after reading highlight', async () => {
      await highlightNode(textNodeIds[0]!);

      // select a subset of nodes after highlight
      const expectedAnchorOffset = 4;
      const expectedFocusOffset = 10;
      await selectNodes(
          afterSelector, expectedAnchorOffset, texts[1]!, expectedFocusOffset,
          texts[2]!);

      assertEquals(textNodeIds[1]!, actualAnchorId);
      assertEquals(textNodeIds[2]!, actualFocusId);
      assertEquals(expectedAnchorOffset, actualAnchorOffset);
      assertEquals(expectedFocusOffset, actualFocusOffset);
    });

    test(
        'selection across previous, current, and after highlight', async () => {
          // highlight middle of node 7
          const highlightStart = 15;
          const highlightEnd = 25;
          await setReadingHighlight(7, highlightStart, 7, highlightEnd);

          // select all text
          const expectedAnchorOffset = 0;
          const expectedFocusOffset = texts[3]!.length;
          const anchorNode = getTextNode(previousSelector, texts[0]!);
          const focusText = texts[3]!;
          const focusNode = getTextNode('p', focusText);
          selection.setBaseAndExtent(
              anchorNode, 0, focusNode, focusText.length);
          await microtasksFinished();

          assertEquals(textNodeIds[0]!, actualAnchorId);
          assertEquals(textNodeIds[3]!, actualFocusId);
          assertEquals(expectedAnchorOffset, actualAnchorOffset);
          assertEquals(expectedFocusOffset, actualFocusOffset);
        });
  });

  suite('app selection is correct when selecting from the main panel: ', () => {
    setup(() => {
      document.onselectionchange = () => {};
    });

    test('one node selected before single node highlight', async () => {
      await highlightNode(5);

      // select a subset of node 3
      const expectedAnchorOffset = 0;
      const expectedFocusOffset = 5;
      fakeTree.setSelection(3, expectedAnchorOffset, 3, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[0]!, selection.anchorNode.textContent);
      assertEquals(texts[0]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('multiple nodes selected before single node highlight', async () => {
      await highlightNode(7);

      // select a subset of nodes 3 and 5
      const expectedAnchorOffset = 1;
      const expectedFocusOffset = 7;
      fakeTree.setSelection(3, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[0]!, selection.anchorNode.textContent);
      assertEquals(texts[1]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('one node selected before multiple node highlight', async () => {
      await setReadingHighlight(7, 0, 9, texts[3]!.length);

      // select a subset of node 5
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 6;
      fakeTree.setSelection(5, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[1]!, selection.anchorNode.textContent);
      assertEquals(texts[1]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('multiple nodes selected before multiple node highlight', async () => {
      await setReadingHighlight(7, 0, 9, texts[3]!.length);

      // select a subset of nodes 3 and 5
      const expectedAnchorOffset = 6;
      const expectedFocusOffset = 2;
      fakeTree.setSelection(3, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[0]!, selection.anchorNode.textContent);
      assertEquals(texts[1]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('one node selected inside single node highlight', async () => {
      const highlightId = 5;
      await highlightNode(highlightId);

      // select a subset of node 5
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      fakeTree.setSelection(
          highlightId, expectedAnchorOffset, highlightId, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[1]!, selection.anchorNode.textContent);
      assertEquals(texts[1]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('one node selected inside multiple node highlight', async () => {
      await setReadingHighlight(3, 0, 5, texts[2]!.length);

      // select a subset of node 5
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 10;
      fakeTree.setSelection(5, expectedAnchorOffset, 5, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[1]!, selection.anchorNode.textContent);
      assertEquals(texts[1]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test(
        'multiple nodes selected inside the multiple node reading highlight',
        async () => {
          await setReadingHighlight(3, 0, 5, texts[2]!.length);

          // select a subset of nodes 3 and 5
          const expectedAnchorOffset = 4;
          const expectedFocusOffset = 9;
          fakeTree.setSelection(
              3, expectedAnchorOffset, 5, expectedFocusOffset);
          app.updateSelection();

          const selection = app.getSelection();
          assertEquals(texts[0]!, selection.anchorNode.textContent);
          assertEquals(texts[1]!, selection.focusNode.textContent);
          assertEquals(expectedAnchorOffset, selection.anchorOffset);
          assertEquals(expectedFocusOffset, selection.focusOffset);
        });

    test('prefix selected in long reading highlight', async () => {
      const highlightId = 7;
      const highlightStart = 5;
      await setReadingHighlight(
          highlightId, highlightStart, highlightId, texts[2]!.length);

      // select node 7 up to where it's highlighted
      const expectedAnchorOffset = 0;
      fakeTree.setSelection(
          highlightId, expectedAnchorOffset, highlightId, highlightStart);
      app.updateSelection();

      const selection = app.getSelection();
      const expectedSelectedText = texts[2]!.slice(0, highlightStart);
      assertEquals(expectedSelectedText, selection.anchorNode.textContent);
      assertEquals(expectedSelectedText, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(highlightStart, selection.focusOffset);
    });

    test('suffix selected in long reading highlight', async () => {
      const highlightId = 7;
      const highlightEnd = 5;
      await setReadingHighlight(highlightId, 0, highlightId, highlightEnd);

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
      assertEquals(expectedSelectedText, selection.anchorNode.textContent);
      assertEquals(expectedSelectedText, selection.focusNode.textContent);
      assertEquals(0, selection.anchorOffset);
      assertEquals(expectedSelectedText.length, selection.focusOffset);
    });

    test('one node selected after reading highlight', async () => {
      await highlightNode(5);

      // select a subset of node 7
      const expectedAnchorOffset = 2;
      const expectedFocusOffset = 8;
      fakeTree.setSelection(7, expectedAnchorOffset, 7, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[2]!, selection.anchorNode.textContent);
      assertEquals(texts[2]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test('multiple nodes selected after reading highlight', async () => {
      await highlightNode(3);

      // select a subset of nodes 5 and 7
      const expectedAnchorOffset = 4;
      const expectedFocusOffset = 10;
      fakeTree.setSelection(5, expectedAnchorOffset, 7, expectedFocusOffset);
      app.updateSelection();

      const selection = app.getSelection();
      assertEquals(texts[1]!, selection.anchorNode.textContent);
      assertEquals(texts[2]!, selection.focusNode.textContent);
      assertEquals(expectedAnchorOffset, selection.anchorOffset);
      assertEquals(expectedFocusOffset, selection.focusOffset);
    });

    test(
        'selection across previous, current, and after highlight', async () => {
          // highlight middle of node 7
          const highlightStart = 15;
          const highlightEnd = 25;
          await setReadingHighlight(7, highlightStart, 7, highlightEnd);

          // select all text
          fakeTree.setSelection(3, 0, 9, texts[3]!.length);
          app.updateSelection();

          const selection = app.getSelection();
          assertEquals(0, selection.anchorOffset);
          assertEquals(texts[3]!.length, selection.focusOffset);
          assertEquals(texts[0]!, selection.anchorNode.textContent);
          assertEquals(texts[3]!, selection.focusNode.textContent);
        });
  });
});
