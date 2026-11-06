// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NodeStore, SelectionController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setupTestEnvironment} from './common.js';
import type {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('SelectionController with translated PDF', () => {
  let selectionController: SelectionController;
  let nodeStore: NodeStore;
  let contentBrowserProxy: TestContentBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;

  // The reading mode content is:
  // <div>                    contentContainer
  //   <p>                    block1
  //     "Bonjour "           firstTextInBlock1
  //     "le monde."          secondTextInBlock1
  //   </p>
  //   <p>                    block2
  //     "Au revoir "         firstTextInBlock2
  //     "mes amis."          secondTextInBlock2
  //   </p>
  // </div>
  // Translating a block replaces its text nodes, e.g. with
  // translatedTextInBlock1.
  let contentContainer: HTMLElement;
  let block1: HTMLElement;
  let firstTextInBlock1: Text;
  let secondTextInBlock1: Text;
  let block2: HTMLElement;
  let secondTextInBlock2: Text;
  let selection: Selection;

  // The AXNodeIDs of the nodes above.
  const block1AxId = 10;
  const firstTextInBlock1AxId = 11;
  const secondTextInBlock1AxId = 12;
  const block2AxId = 20;
  const firstTextInBlock2AxId = 21;
  const secondTextInBlock2AxId = 22;

  // Creates a text node in a block the way ContentController does for PDFs.
  // Text nodes must be created in document order.
  function createTextInBlock(
      text: string, axId: number, blockAxId: number): Text {
    const textNode = document.createTextNode(text);
    nodeStore.setDomNode(textNode, axId);
    nodeStore.registerTextInBlock(axId, blockAxId, text.length);
    return textNode;
  }

  // Creates a block the way ContentController does for PDFs.
  function createBlock(axId: number, textNodes: Text[]): HTMLElement {
    const block = document.createElement('p');
    nodeStore.setDomNode(block, axId);
    block.append(...textNodes);
    return block;
  }

  // Replaces the text in the block with the translation the way the
  // translate script does, which detaches the original text nodes.
  function translate(block: HTMLElement, translation: string): Text {
    const font = document.createElement('font');
    const textNode = document.createTextNode(translation);
    font.appendChild(textNode);
    block.replaceChildren(font);
    return textNode;
  }

  function selectInMainPanel(
      startId: number, startOffset: number, endId: number, endOffset: number) {
    contentBrowserProxy.startNodeId = startId;
    contentBrowserProxy.startOffset = startOffset;
    contentBrowserProxy.endNodeId = endId;
    contentBrowserProxy.endOffset = endOffset;
  }

  function selectInReadingMode(
      anchorNode: Node, anchorOffset: number, focusNode: Node,
      focusOffset: number) {
    selection.setBaseAndExtent(
        anchorNode, anchorOffset, focusNode, focusOffset);
    selectionController.onSelectionChange(selection, contentContainer);
  }

  function assertLastSentSelection(
      anchorId: number, anchorOffset: number, focusId: number,
      focusOffset: number) {
    const sent = contentBrowserProxy.getArgs('onSelectionChange');
    assertTrue(sent.length > 0);
    assertDeepEquals(
        [anchorId, anchorOffset, focusId, focusOffset], sent[sent.length - 1]);
  }

  function assertSelection(
      anchorNode: Node, anchorOffset: number, focusNode: Node,
      focusOffset: number) {
    assertEquals(anchorNode, selection.anchorNode);
    assertEquals(anchorOffset, selection.anchorOffset);
    assertEquals(focusNode, selection.focusNode);
    assertEquals(focusOffset, selection.focusOffset);
  }

  // Returns the part of the block's text that's selected.
  function getSelectedTextIn(block: Node): string {
    const selected = document.createRange();
    selected.selectNodeContents(block);
    const range = selection.getRangeAt(0);
    if (range.compareBoundaryPoints(Range.START_TO_START, selected) > 0) {
      selected.setStart(range.startContainer, range.startOffset);
    }
    if (range.compareBoundaryPoints(Range.END_TO_END, selected) < 0) {
      selected.setEnd(range.endContainer, range.endOffset);
    }
    return selected.toString();
  }

  // Asserts that all of the text in the given blocks is selected, and no text
  // in the other blocks. This checks the selected text rather than the
  // range's endpoints, since the shadow root's selection canonicalizes them.
  function assertSelectedBlocks(...selectedBlocks: HTMLElement[]) {
    assertEquals(1, selection.rangeCount);
    for (const block of [block1, block2]) {
      const expected =
          selectedBlocks.includes(block) ? block.textContent : '';
      assertEquals(expected, getSelectedTextIn(block));
    }
  }

  setup(() => {
    const result = setupTestEnvironment();
    contentBrowserProxy = result.contentBrowserProxy;
    visualBrowserProxy = result.visualBrowserProxy;
    nodeStore = result.nodeStore;
    selectionController = result.selectionController;

    visualBrowserProxy.pdf = true;
    contentBrowserProxy.translatePdfEnabled = true;
    contentBrowserProxy.contentTranslated = true;

    firstTextInBlock1 =
        createTextInBlock('Bonjour ', firstTextInBlock1AxId, block1AxId);
    secondTextInBlock1 =
        createTextInBlock('le monde.', secondTextInBlock1AxId, block1AxId);
    block1 = createBlock(block1AxId, [firstTextInBlock1, secondTextInBlock1]);
    const firstTextInBlock2 =
        createTextInBlock('Au revoir ', firstTextInBlock2AxId, block2AxId);
    secondTextInBlock2 =
        createTextInBlock('mes amis.', secondTextInBlock2AxId, block2AxId);
    block2 = createBlock(block2AxId, [firstTextInBlock2, secondTextInBlock2]);
    contentContainer = document.createElement('div');
    contentContainer.append(block1, block2);
    // Reading mode's content is in the app's shadow root, and the app gets the
    // selection from there (see AppElement.getSelection()). Unlike the
    // document's selection, a shadow root's selection reports canonicalized
    // endpoints, e.g. in the blocks' text rather than in their parent.
    const host = document.createElement('div');
    document.body.appendChild(host);
    const shadowRoot = host.attachShadow({mode: 'open'});
    shadowRoot.appendChild(contentContainer);

    const shadowSelection = shadowRoot.getSelection();
    assertTrue(!!shadowSelection);
    shadowSelection.removeAllRanges();
    selection = shadowSelection;
  });

  suite('onSelectionChange', () => {
    test('keeps the reading mode selection as is', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');

      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);

      // Only the main panel selection is extended to whole blocks.
      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);
      assertSelection(translatedTextInBlock1, 3, translatedTextInBlock1, 8);
    });

    test('sends all text in the block with the selection', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');

      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);

      assertEquals(1, contentBrowserProxy.getCallCount('onSelectionChange'));
      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);
    });

    test('sends all text in the blocks across the selection', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      const translatedTextInBlock2 = translate(block2, 'Goodbye my friends.');

      selectInReadingMode(translatedTextInBlock1, 6, translatedTextInBlock2, 4);

      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock2AxId,
          secondTextInBlock2.length);
    });

    test('sends a backward selection forward', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      const translatedTextInBlock2 = translate(block2, 'Goodbye my friends.');

      selectInReadingMode(translatedTextInBlock2, 4, translatedTextInBlock1, 6);

      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock2AxId,
          secondTextInBlock2.length);
    });

    test('sends a selection with element endpoints', () => {
      translate(block1, 'Hello world.');
      translate(block2, 'Goodbye my friends.');

      selectInReadingMode(contentContainer, 0, contentContainer, 1);

      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);
    });

    test('sends a selection with element endpoints in different blocks', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      translate(block2, 'Goodbye my friends.');

      // Backward, from the end of block 2 to inside block 1's text.
      selectInReadingMode(block2, 1, translatedTextInBlock1, 6);

      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock2AxId,
          secondTextInBlock2.length);
    });

    test('sends a selection from a block into its own text', () => {
      translate(block1, 'Hello world.');
      const translatedTextInBlock2 = translate(block2, 'Goodbye my friends.');

      // From the start of block 2 to inside its text.
      selectInReadingMode(block2, 0, translatedTextInBlock2, 4);

      assertLastSentSelection(
          firstTextInBlock2AxId, 0, secondTextInBlock2AxId,
          secondTextInBlock2.length);
      // TODO(crbug.com/578130985): Also test backward selections where
      // one endpoint's node contains the other's, e.g. from inside block
      // 2's text to the start of block 2.
    });

    test('skips blocks without selected text', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      const translatedTextInBlock2 = translate(block2, 'Goodbye my friends.');

      // Starts after the last character of the first block.
      selectInReadingMode(
          translatedTextInBlock1, translatedTextInBlock1.length,
          translatedTextInBlock2, 4);

      assertLastSentSelection(
          firstTextInBlock2AxId, 0, secondTextInBlock2AxId,
          secondTextInBlock2.length);
    });

    test('collapses the main panel selection if no text is selected', () => {
      translate(block1, 'Hello world.');
      translate(block2, 'Goodbye my friends.');
      const whitespace = document.createTextNode('\n');
      contentContainer.insertBefore(whitespace, block2);

      selectInReadingMode(whitespace, 0, whitespace, 1);

      assertEquals(0, contentBrowserProxy.getCallCount('onSelectionChange'));
      assertEquals(1, contentBrowserProxy.getCallCount('onCollapseSelection'));
    });

    test('sends the same blocks on every selection change', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');

      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 5);
      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);

      // ReadAnythingAppController drops the repeat while the first is in
      // flight.
      assertEquals(2, contentBrowserProxy.getCallCount('onSelectionChange'));
      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);
    });

    test('does not send the main panel selection', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      selectInMainPanel(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);

      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);

      assertEquals(0, contentBrowserProxy.getCallCount('onSelectionChange'));
    });

    test('does not send without user activation', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      contentBrowserProxy.transientUserActivation = false;

      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 5);
      assertEquals(0, contentBrowserProxy.getCallCount('onSelectionChange'));

      contentBrowserProxy.transientUserActivation = true;
      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);
      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);
    });

    test('does not send without focus', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      contentBrowserProxy.focused = false;

      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);

      assertEquals(0, contentBrowserProxy.getCallCount('onSelectionChange'));
      assertEquals(0, contentBrowserProxy.getCallCount('onCollapseSelection'));
    });

    test('sends blocks if translated, even for original text', () => {
      selectInReadingMode(firstTextInBlock1, 2, firstTextInBlock1, 5);

      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);
    });

    test('does not send blocks before translation ends', () => {
      contentBrowserProxy.contentTranslated = false;
      const translatedTextInBlock1 = translate(block1, 'Hello world.');

      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);

      // The selection is mapped exactly, and translated text doesn't map to
      // the main panel's text.
      assertEquals(0, contentBrowserProxy.getCallCount('onSelectionChange'));
      assertEquals(1, contentBrowserProxy.getCallCount('onCollapseSelection'));
    });

    test('sends the exact selection if not translated', () => {
      contentBrowserProxy.contentTranslated = false;

      selectInReadingMode(firstTextInBlock1, 2, firstTextInBlock1, 5);

      assertLastSentSelection(
          firstTextInBlock1AxId, 2, firstTextInBlock1AxId, 5);
    });

    test('sends the exact selection if disabled', () => {
      contentBrowserProxy.translatePdfEnabled = false;

      selectInReadingMode(firstTextInBlock1, 2, firstTextInBlock1, 5);

      assertLastSentSelection(
          firstTextInBlock1AxId, 2, firstTextInBlock1AxId, 5);
    });

    test('sends the exact selection if not a PDF', () => {
      visualBrowserProxy.pdf = false;

      selectInReadingMode(firstTextInBlock1, 2, firstTextInBlock1, 5);

      assertLastSentSelection(
          firstTextInBlock1AxId, 2, firstTextInBlock1AxId, 5);
    });
  });

  suite('updateSelection', () => {
    test('selects the blocks with the main panel selection', () => {
      translate(block1, 'Hello world.');
      translate(block2, 'Goodbye my friends.');
      selectInMainPanel(secondTextInBlock1AxId, 1, firstTextInBlock2AxId, 2);

      selectionController.updateSelection(selection, contentContainer);

      assertSelectedBlocks(block1, block2);
    });

    test('selects only the blocks with selected text', () => {
      translate(block1, 'Hello world.');
      translate(block2, 'Goodbye my friends.');
      selectInMainPanel(
          secondTextInBlock1AxId, secondTextInBlock1.length,
          secondTextInBlock2AxId, 2);

      selectionController.updateSelection(selection, contentContainer);

      assertSelectedBlocks(block2);
    });

    test('clears the selection if no text in a block is selected', () => {
      translate(block1, 'Hello world.');
      translate(block2, 'Goodbye my friends.');
      selection.selectAllChildren(block1);
      selectInMainPanel(
          secondTextInBlock1AxId, secondTextInBlock1.length,
          firstTextInBlock2AxId, 0);

      selectionController.updateSelection(selection, contentContainer);

      assertEquals(0, selection.rangeCount);
    });

    test('does not send the drawn selection to the main panel', () => {
      translate(block1, 'Hello world.');
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);

      selectionController.updateSelection(selection, contentContainer);
      // The selection change caused by drawing the selection.
      selectionController.onSelectionChange(selection, contentContainer);

      assertSelectedBlocks(block1);
      assertEquals(0, contentBrowserProxy.getCallCount('onSelectionChange'));
    });

    test('does not redraw the same blocks', () => {
      const translatedTextInBlock1 = translate(block1, 'Hello world.');
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);
      selectionController.updateSelection(selection, contentContainer);
      // The selection change caused by drawing the selection.
      selectionController.onSelectionChange(selection, contentContainer);

      selectInMainPanel(firstTextInBlock1AxId, 2, secondTextInBlock1AxId, 3);
      selectionController.updateSelection(selection, contentContainer);

      assertSelectedBlocks(block1);
      // Since the selection wasn't redrawn, the next selection change is
      // from the user and is sent to the main panel.
      selectInReadingMode(translatedTextInBlock1, 3, translatedTextInBlock1, 8);
      assertLastSentSelection(
          firstTextInBlock1AxId, 0, secondTextInBlock1AxId,
          secondTextInBlock1.length);
    });

    test('selects blocks if translated, even for original text', () => {
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);

      selectionController.updateSelection(selection, contentContainer);

      assertSelectedBlocks(block1);
    });

    test('does not select blocks before translation ends', () => {
      contentBrowserProxy.contentTranslated = false;
      translate(block1, 'Hello world.');
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);

      selectionController.updateSelection(selection, contentContainer);

      // The selection is drawn exactly, and the original text isn't shown.
      assertEquals(0, selection.rangeCount);
    });

    test('selects the exact text if not translated', () => {
      contentBrowserProxy.contentTranslated = false;
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);

      selectionController.updateSelection(selection, contentContainer);

      assertSelection(firstTextInBlock1, 1, firstTextInBlock1, 4);
    });

    test('selects the exact text if disabled', () => {
      contentBrowserProxy.translatePdfEnabled = false;
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);

      selectionController.updateSelection(selection, contentContainer);

      assertSelection(firstTextInBlock1, 1, firstTextInBlock1, 4);
    });
  });

  suite('onTranslationStateChanged', () => {
    test('redraws the main panel selection as blocks', () => {
      contentBrowserProxy.contentTranslated = false;
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);
      selectionController.updateSelection(selection, contentContainer);
      assertSelection(firstTextInBlock1, 1, firstTextInBlock1, 4);

      translate(block1, 'Hello world.');
      contentBrowserProxy.contentTranslated = true;
      selectionController.onTranslationStateChanged(
          selection, contentContainer);

      assertSelectedBlocks(block1);
    });

    test('redraws the exact main panel selection when restored', () => {
      const originalNodes = Array.from(block1.childNodes);
      translate(block1, 'Hello world.');
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);
      selectionController.updateSelection(selection, contentContainer);
      assertSelectedBlocks(block1);

      block1.replaceChildren(...originalNodes);
      contentBrowserProxy.contentTranslated = false;
      selectionController.onTranslationStateChanged(
          selection, contentContainer);

      assertSelection(firstTextInBlock1, 1, firstTextInBlock1, 4);
    });

    test('does nothing if disabled', () => {
      contentBrowserProxy.translatePdfEnabled = false;
      selectInMainPanel(firstTextInBlock1AxId, 1, firstTextInBlock1AxId, 4);
      selection.setBaseAndExtent(firstTextInBlock1, 2, firstTextInBlock1, 5);

      selectionController.onTranslationStateChanged(
          selection, contentContainer);

      assertSelection(firstTextInBlock1, 2, firstTextInBlock1, 5);
    });
  });
});
