// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {NodeStore, ReadingModeSelection, SelectionWithIds} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TranslatedSelectionMapper} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals, assertNull, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setupTestEnvironment} from './common.js';
import type {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('TranslatedSelectionMapper', () => {
  let nodeStore: NodeStore;
  let contentBrowserProxy: TestContentBrowserProxy;
  let visualBrowserProxy: TestVisualBrowserProxy;
  let mapper: TranslatedSelectionMapper;

  // The reading mode content is:
  // <div>                    contentContainer
  //   <p>                    block1
  //     "Hello "             textInBlock1
  //     <a>"world"</a>       linkInBlock1 with textInLink1
  //   </p>
  //   "\n"                   whitespaceBetweenBlocks
  //   <p>"Second"</p>        block2 with textInBlock2
  //   <span>"Loose"</span>   spanOutsideBlocks with textOutsideBlocks
  //   <p>"Third"</p>         block3 with textInBlock3
  // </div>
  let contentContainer: HTMLElement;
  let block1: HTMLElement;
  let textInBlock1: Text;
  let textInLink1: Text;
  let whitespaceBetweenBlocks: Text;
  let block2: HTMLElement;
  let textInBlock2: Text;
  let textOutsideBlocks: Text;
  let textInBlock3: Text;

  // The AXNodeIDs of the nodes above.
  const block1AxId = 10;
  const textInBlock1AxId = 11;
  const linkInBlock1AxId = 12;
  const textInLink1AxId = 13;
  const block2AxId = 20;
  const textInBlock2AxId = 21;
  const block3AxId = 30;
  const textInBlock3AxId = 31;
  const spanOutsideBlocksAxId = 40;
  const textOutsideBlocksAxId = 41;

  function createText(text: string, axId: number): Text {
    const node = document.createTextNode(text);
    nodeStore.setDomNode(node, axId);
    return node;
  }

  function createElement(
      tag: string, axId: number, children: Node[]): HTMLElement {
    const element = document.createElement(tag);
    nodeStore.setDomNode(element, axId);
    element.append(...children);
    return element;
  }

  function select(
      anchorNode: Node, anchorOffset: number, focusNode: Node,
      focusOffset: number): ReadingModeSelection {
    return {anchorNode, anchorOffset, focusNode, focusOffset};
  }

  function selectIds(
      anchorNodeId: number, anchorOffset: number, focusNodeId: number,
      focusOffset: number): SelectionWithIds {
    return {anchorNodeId, anchorOffset, focusNodeId, focusOffset};
  }

  function assertSelection(
      expected: ReadingModeSelection, actual: ReadingModeSelection|null) {
    assertTrue(!!actual);
    assertEquals(expected.anchorNode, actual.anchorNode);
    assertEquals(expected.anchorOffset, actual.anchorOffset);
    assertEquals(expected.focusNode, actual.focusNode);
    assertEquals(expected.focusOffset, actual.focusOffset);
  }

  setup(() => {
    const testEnvironment = setupTestEnvironment();
    nodeStore = testEnvironment.nodeStore;
    contentBrowserProxy = testEnvironment.contentBrowserProxy;
    visualBrowserProxy = testEnvironment.visualBrowserProxy;
    // The block map and the mapper are only used for PDFs when TranslatePdf is
    // enabled.
    contentBrowserProxy.translatePdfEnabled = true;
    visualBrowserProxy.pdf = true;
    mapper = new TranslatedSelectionMapper();

    textInBlock1 = createText('Hello ', textInBlock1AxId);
    textInLink1 = createText('world', textInLink1AxId);
    const linkInBlock1 = createElement('a', linkInBlock1AxId, [textInLink1]);
    block1 = createElement('p', block1AxId, [textInBlock1, linkInBlock1]);
    whitespaceBetweenBlocks = document.createTextNode('\n');
    textInBlock2 = createText('Second', textInBlock2AxId);
    block2 = createElement('p', block2AxId, [textInBlock2]);
    textOutsideBlocks = createText('Loose', textOutsideBlocksAxId);
    const spanOutsideBlocks =
        createElement('span', spanOutsideBlocksAxId, [textOutsideBlocks]);
    textInBlock3 = createText('Third', textInBlock3AxId);
    const block3 = createElement('p', block3AxId, [textInBlock3]);
    contentContainer = document.createElement('div');
    contentContainer.append(
        block1, whitespaceBetweenBlocks, block2, spanOutsideBlocks, block3);
    document.body.appendChild(contentContainer);

    nodeStore.registerTextInBlock(
        textInBlock1AxId, block1AxId, textInBlock1.length);
    nodeStore.registerTextInBlock(
        textInLink1AxId, block1AxId, textInLink1.length);
    nodeStore.registerTextInBlock(
        textInBlock2AxId, block2AxId, textInBlock2.length);
    nodeStore.registerTextInBlock(
        textInBlock3AxId, block3AxId, textInBlock3.length);
  });

  teardown(() => {
    contentContainer.remove();
  });

  suite('getAxSelection', () => {
    // The AX selections of all of the text in block 1, in blocks 1 through 2,
    // in block 2, and in blocks 2 through 3.
    let block1Selection: SelectionWithIds;
    let blocks1To2Selection: SelectionWithIds;
    let block2Selection: SelectionWithIds;
    let blocks2To3Selection: SelectionWithIds;

    setup(() => {
      block1Selection =
          selectIds(textInBlock1AxId, 0, textInLink1AxId, textInLink1.length);
      blocks1To2Selection =
          selectIds(textInBlock1AxId, 0, textInBlock2AxId, textInBlock2.length);
      block2Selection =
          selectIds(textInBlock2AxId, 0, textInBlock2AxId, textInBlock2.length);
      blocks2To3Selection =
          selectIds(textInBlock2AxId, 0, textInBlock3AxId, textInBlock3.length);
    });

    test('selects all text in the block with the selection', () => {
      assertDeepEquals(
          block1Selection,
          mapper.getAxSelection(
              select(textInBlock1, 1, textInBlock1, 3), contentContainer));
    });

    test('selects all text in the blocks across the selection', () => {
      assertDeepEquals(
          blocks1To2Selection,
          mapper.getAxSelection(
              select(textInLink1, 2, textInBlock2, 3), contentContainer));
    });

    test('returns a forward selection for a backward selection', () => {
      assertDeepEquals(
          blocks1To2Selection,
          mapper.getAxSelection(
              select(textInBlock2, 3, textInBlock1, 1), contentContainer));
    });

    test('handles element endpoints', () => {
      // From before block 1 to after block 2.
      assertDeepEquals(
          blocks1To2Selection,
          mapper.getAxSelection(
              select(contentContainer, 0, contentContainer, 3),
              contentContainer));
    });

    test('handles element endpoints in different blocks', () => {
      // From the start of block 1 to inside textInBlock2.
      assertDeepEquals(
          blocks1To2Selection,
          mapper.getAxSelection(
              select(block1, 0, textInBlock2, 3), contentContainer));
      // From inside textInLink1 to the end of block 2.
      assertDeepEquals(
          blocks1To2Selection,
          mapper.getAxSelection(
              select(textInLink1, 2, block2, 1), contentContainer));
      // Backward, from the end of block 2 to inside textInBlock1.
      assertDeepEquals(
          blocks1To2Selection,
          mapper.getAxSelection(
              select(block2, 1, textInBlock1, 1), contentContainer));
    });

    test('handles element endpoints that contain the other endpoint', () => {
      // From inside textInLink1 to the end of block 1, which contains
      // textInLink1.
      assertDeepEquals(
          block1Selection,
          mapper.getAxSelection(
              select(textInLink1, 2, block1, 2), contentContainer));
      // From the start of block 2 to inside textInBlock2, which block 2
      // contains.
      assertDeepEquals(
          block2Selection,
          mapper.getAxSelection(
              select(block2, 0, textInBlock2, 3), contentContainer));
      // TODO(crbug.com/578130985): Also test backward selections, e.g. from
      // inside textInLink1 to the start of block 1, or from the end of block 2
      // to inside textInBlock2. isBackwardSelection treats them as forward
      // when one endpoint's node contains the other's, so they currently
      // return null.
    });

    test('skips blocks without selected text', () => {
      // Starts at the end of block 1.
      assertDeepEquals(
          blocks2To3Selection,
          mapper.getAxSelection(
              select(textInLink1, textInLink1.length, textInBlock3, 2),
              contentContainer));
      // Ends at the start of block 2.
      assertDeepEquals(
          block1Selection,
          mapper.getAxSelection(
              select(textInBlock1, 1, textInBlock2, 0), contentContainer));
    });

    test('handles endpoints in whitespace between blocks', () => {
      // Ends in the whitespace after block 1.
      assertDeepEquals(
          block1Selection,
          mapper.getAxSelection(
              select(textInBlock1, 1, whitespaceBetweenBlocks, 1),
              contentContainer));
      // Starts in the whitespace before block 2.
      assertDeepEquals(
          block2Selection,
          mapper.getAxSelection(
              select(whitespaceBetweenBlocks, 0, textInBlock2, 3),
              contentContainer));
    });

    test('handles endpoints in whitespace in another block', () => {
      const leadingWhitespaceInBlock2 = document.createTextNode('  ');
      block2.prepend(leadingWhitespaceInBlock2);

      // Ends in the whitespace at the start of block 2, so block 2 isn't
      // included.
      assertDeepEquals(
          block1Selection,
          mapper.getAxSelection(
              select(textInBlock1, 1, leadingWhitespaceInBlock2, 1),
              contentContainer));
      // Starts in the same whitespace, so block 2 is included because its
      // text is selected.
      assertDeepEquals(
          blocks2To3Selection,
          mapper.getAxSelection(
              select(leadingWhitespaceInBlock2, 1, textInBlock3, 2),
              contentContainer));
    });

    test('returns null if no text is selected', () => {
      // Collapsed.
      assertNull(mapper.getAxSelection(
          select(textInBlock1, 2, textInBlock1, 2), contentContainer));
      // Only whitespace between blocks.
      assertNull(mapper.getAxSelection(
          select(whitespaceBetweenBlocks, 0, whitespaceBetweenBlocks, 1),
          contentContainer));
    });

    test('returns null if only text outside of blocks is selected', () => {
      assertNull(mapper.getAxSelection(
          select(textOutsideBlocks, 0, textOutsideBlocks, 3),
          contentContainer));
    });

    test('maps translated text by its block', () => {
      // Translation replaces the text nodes but keeps the block element.
      const translated = document.createTextNode('Bonjour le monde');
      block1.replaceChildren(translated);

      assertDeepEquals(
          block1Selection,
          mapper.getAxSelection(
              select(translated, 0, translated, 7), contentContainer));
    });
  });

  suite('getBlockAlignedSelection', () => {
    test('selects whole blocks', () => {
      assertSelection(
          select(contentContainer, 0, contentContainer, 3),
          mapper.getBlockAlignedSelection(
              select(textInLink1, 1, textInBlock2, 2), contentContainer));
    });

    test('keeps the direction of the selection', () => {
      assertSelection(
          select(contentContainer, 3, contentContainer, 0),
          mapper.getBlockAlignedSelection(
              select(textInBlock2, 2, textInLink1, 1), contentContainer));
    });

    test('handles element endpoints', () => {
      // From the start of block 1 to inside textInBlock2.
      assertSelection(
          select(contentContainer, 0, contentContainer, 3),
          mapper.getBlockAlignedSelection(
              select(block1, 0, textInBlock2, 3), contentContainer));
      // Backward, from the end of block 2 to inside textInBlock1.
      assertSelection(
          select(contentContainer, 3, contentContainer, 0),
          mapper.getBlockAlignedSelection(
              select(block2, 1, textInBlock1, 1), contentContainer));
      // From inside textInLink1 to the end of block 1, which contains
      // textInLink1.
      assertSelection(
          select(contentContainer, 0, contentContainer, 1),
          mapper.getBlockAlignedSelection(
              select(textInLink1, 2, block1, 2), contentContainer));
      // TODO(crbug.com/578130985): Also test backward selections where one
      // endpoint's node contains the other's, e.g. from inside textInLink1 to
      // the start of block 1.
    });

    test('returns null if no text is selected', () => {
      assertNull(mapper.getBlockAlignedSelection(
          select(whitespaceBetweenBlocks, 0, whitespaceBetweenBlocks, 1),
          contentContainer));
    });

    test('handles triple-click selections', () => {
      // Triple-clicking a paragraph can end the selection at the start of the
      // next block, which doesn't include the next block.
      assertSelection(
          select(contentContainer, 0, contentContainer, 1),
          mapper.getBlockAlignedSelection(
              select(textInBlock1, 0, block2, 0), contentContainer));
      // It can also put both endpoints in the paragraph's element.
      assertSelection(
          select(contentContainer, 0, contentContainer, 1),
          mapper.getBlockAlignedSelection(
              select(block1, 0, block1, 2), contentContainer));
      // The last paragraph has no next block, so the selection can end after it
      // in its parent.
      assertSelection(
          select(contentContainer, 4, contentContainer, 5),
          mapper.getBlockAlignedSelection(
              select(textInBlock3, 0, contentContainer, 5), contentContainer));
    });

    test('maps translated text by its block', () => {
      // Translation replaces the text nodes with text nodes that aren't in the
      // NodeStore, sometimes wrapped in other elements, but keeps the block
      // elements.
      const translatedTextInBlock1 =
          document.createTextNode('Bonjour le monde');
      const font = document.createElement('font');
      font.appendChild(translatedTextInBlock1);
      block1.replaceChildren(font);
      const translatedTextInBlock2 = document.createTextNode('Seconde');
      block2.replaceChildren(translatedTextInBlock2);

      assertSelection(
          select(contentContainer, 0, contentContainer, 3),
          mapper.getBlockAlignedSelection(
              select(translatedTextInBlock1, 3, translatedTextInBlock2, 2),
              contentContainer));
      // Backward.
      assertSelection(
          select(contentContainer, 3, contentContainer, 0),
          mapper.getBlockAlignedSelection(
              select(translatedTextInBlock2, 2, translatedTextInBlock1, 3),
              contentContainer));
    });
  });

  suite('getReadingModeSelection', () => {
    test('selects the whole block with the selection', () => {
      const result = mapper.getReadingModeSelection(
          selectIds(textInBlock1AxId, 1, textInBlock1AxId, 3),
          contentContainer);

      assertSelection(select(contentContainer, 0, contentContainer, 1), result);
      // The first block is the anchor's child at the anchor offset.
      assertTrue(!!result);
      assertEquals(block1, result.anchorNode.childNodes[result.anchorOffset]);
    });

    test('selects the whole blocks across the selection', () => {
      // From block 1 through block 3.
      assertSelection(
          select(contentContainer, 0, contentContainer, 5),
          mapper.getReadingModeSelection(
              selectIds(textInLink1AxId, 2, textInBlock3AxId, 1),
              contentContainer));
    });

    test('stays valid when the blocks are translated', () => {
      const result = mapper.getReadingModeSelection(
          selectIds(textInBlock1AxId, 1, textInBlock1AxId, 3),
          contentContainer);
      assertTrue(!!result);
      const selection = document.getSelection()!;
      selection.setBaseAndExtent(
          result.anchorNode, result.anchorOffset, result.focusNode,
          result.focusOffset);

      block1.replaceChildren(document.createTextNode('Bonjour le monde'));

      assertEquals(contentContainer, selection.anchorNode);
      assertEquals(0, selection.anchorOffset);
      assertEquals(contentContainer, selection.focusNode);
      assertEquals(1, selection.focusOffset);
      assertStringContains(selection.toString(), 'Bonjour le monde');
    });

    test('returns null if no text is selected', () => {
      // From the end of textInBlock1 to the start of textInLink1.
      assertNull(mapper.getReadingModeSelection(
          selectIds(textInBlock1AxId, textInBlock1.length, textInLink1AxId, 0),
          contentContainer));
    });

    test('returns null if an endpoint has no node id', () => {
      assertNull(mapper.getReadingModeSelection(
          {anchorOffset: 1, focusNodeId: textInBlock1AxId, focusOffset: 3},
          contentContainer));
      assertNull(mapper.getReadingModeSelection(
          {anchorNodeId: textInBlock1AxId, anchorOffset: 1, focusOffset: 3},
          contentContainer));
    });

    test('returns null if the text is not in a block', () => {
      assertNull(mapper.getReadingModeSelection(
          selectIds(textOutsideBlocksAxId, 0, textOutsideBlocksAxId, 3),
          contentContainer));
    });

    test('returns null if a block is not shown', () => {
      block2.remove();

      assertNull(mapper.getReadingModeSelection(
          selectIds(textInBlock2AxId, 0, textInBlock2AxId, 3),
          contentContainer));
    });
  });

  suite('with TranslatePdf disabled', () => {
    setup(() => {
      contentBrowserProxy.translatePdfEnabled = false;
    });

    test('getAxSelection returns null', () => {
      assertNull(mapper.getAxSelection(
          select(textInBlock1, 1, textInBlock1, 3), contentContainer));
    });

    test('getBlockAlignedSelection returns null', () => {
      assertNull(mapper.getBlockAlignedSelection(
          select(textInLink1, 1, textInBlock2, 2), contentContainer));
    });

    test('getReadingModeSelection returns null', () => {
      assertNull(mapper.getReadingModeSelection(
          selectIds(textInBlock1AxId, 1, textInBlock1AxId, 3),
          contentContainer));
    });
  });

  suite('when the page is not a PDF', () => {
    setup(() => {
      visualBrowserProxy.pdf = false;
    });

    test('getAxSelection returns null', () => {
      assertNull(mapper.getAxSelection(
          select(textInBlock1, 1, textInBlock1, 3), contentContainer));
    });

    test('getBlockAlignedSelection returns null', () => {
      assertNull(mapper.getBlockAlignedSelection(
          select(textInLink1, 1, textInBlock2, 2), contentContainer));
    });

    test('getReadingModeSelection returns null', () => {
      assertNull(mapper.getReadingModeSelection(
          selectIds(textInBlock1AxId, 1, textInBlock1AxId, 3),
          contentContainer));
    });
  });
});
