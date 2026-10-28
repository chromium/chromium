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
  //   <p>                    block 10
  //     "Hello "             text 11
  //     <a>"world"</a>       link 12 with text 13
  //   </p>
  //   "\n"                   whitespace between blocks
  //   <p>"Second"</p>        block 20 with text 21
  //   <span>"Loose"</span>   span 40 with text 41, which isn't in a block
  //   <p>"Third"</p>         block 30 with text 31
  // </div>
  let contentContainer: HTMLElement;
  let block10: HTMLElement;
  let text11: Text;
  let text13: Text;
  let whitespace: Text;
  let block20: HTMLElement;
  let text21: Text;
  let text41: Text;
  let text31: Text;

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

    text11 = createText('Hello ', 11);
    text13 = createText('world', 13);
    block10 =
        createElement('p', 10, [text11, createElement('a', 12, [text13])]);
    whitespace = document.createTextNode('\n');
    text21 = createText('Second', 21);
    block20 = createElement('p', 20, [text21]);
    text41 = createText('Loose', 41);
    const span40 = createElement('span', 40, [text41]);
    text31 = createText('Third', 31);
    const block30 = createElement('p', 30, [text31]);
    contentContainer = document.createElement('div');
    contentContainer.append(block10, whitespace, block20, span40, block30);
    document.body.appendChild(contentContainer);

    nodeStore.registerTextInBlock(11, 10, text11.length);
    nodeStore.registerTextInBlock(13, 10, text13.length);
    nodeStore.registerTextInBlock(21, 20, text21.length);
    nodeStore.registerTextInBlock(31, 30, text31.length);
  });

  teardown(() => {
    contentContainer.remove();
  });

  suite('getAxSelection', () => {
    test('selects all text in the block with the selection', () => {
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 13, focusOffset: 5},
          mapper.getAxSelection(
              select(text11, 1, text11, 3), contentContainer));
    });

    test('selects all text in the blocks across the selection', () => {
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(text13, 2, text21, 3), contentContainer));
    });

    test('returns a forward selection for a backward selection', () => {
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(text21, 3, text11, 1), contentContainer));
    });

    test('handles element endpoints', () => {
      // From before block 10 to after block 20.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(contentContainer, 0, contentContainer, 3),
              contentContainer));
    });

    test('handles element endpoints in different blocks', () => {
      // From the start of block 10 to inside text 21.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(block10, 0, text21, 3), contentContainer));
      // From inside text 13 to the end of block 20.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(text13, 2, block20, 1), contentContainer));
      // Backward, from the end of block 20 to inside text 11.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(block20, 1, text11, 1), contentContainer));
    });

    test('handles element endpoints that contain the other endpoint', () => {
      // From inside text 13 to the end of block 10, which contains text 13.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 13, focusOffset: 5},
          mapper.getAxSelection(
              select(text13, 2, block10, 2), contentContainer));
      // From the start of block 20 to inside text 21, which block 20 contains.
      assertDeepEquals(
          {anchorNodeId: 21, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(block20, 0, text21, 3), contentContainer));
      // TODO(crbug.com/578130985): Also test backward selections, e.g. from
      // inside text 13 to the start of block 10, or from the end of block 20 to
      // inside text 21. isBackwardSelection treats them as forward when one
      // endpoint's node contains the other's, so they currently return null.
    });

    test('skips blocks without selected text', () => {
      // Starts at the end of block 10.
      assertDeepEquals(
          {anchorNodeId: 21, anchorOffset: 0, focusNodeId: 31, focusOffset: 5},
          mapper.getAxSelection(
              select(text13, text13.length, text31, 2), contentContainer));
      // Ends at the start of block 20.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 13, focusOffset: 5},
          mapper.getAxSelection(
              select(text11, 1, text21, 0), contentContainer));
    });

    test('handles endpoints in whitespace between blocks', () => {
      // Ends in the whitespace after block 10.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 13, focusOffset: 5},
          mapper.getAxSelection(
              select(text11, 1, whitespace, 1), contentContainer));
      // Starts in the whitespace before block 20.
      assertDeepEquals(
          {anchorNodeId: 21, anchorOffset: 0, focusNodeId: 21, focusOffset: 6},
          mapper.getAxSelection(
              select(whitespace, 0, text21, 3), contentContainer));
    });

    test('handles endpoints in whitespace in another block', () => {
      const leading = document.createTextNode('  ');
      block20.prepend(leading);

      // Ends in the whitespace at the start of block 20, so block 20 isn't
      // included.
      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 13, focusOffset: 5},
          mapper.getAxSelection(
              select(text11, 1, leading, 1), contentContainer));
      // Starts in the same whitespace, so block 20 is included because its
      // text is selected.
      assertDeepEquals(
          {anchorNodeId: 21, anchorOffset: 0, focusNodeId: 31, focusOffset: 5},
          mapper.getAxSelection(
              select(leading, 1, text31, 2), contentContainer));
    });

    test('returns null if no text is selected', () => {
      // Collapsed.
      assertNull(mapper.getAxSelection(
          select(text11, 2, text11, 2), contentContainer));
      // Only whitespace between blocks.
      assertNull(mapper.getAxSelection(
          select(whitespace, 0, whitespace, 1), contentContainer));
    });

    test('returns null if only text outside of blocks is selected', () => {
      assertNull(mapper.getAxSelection(
          select(text41, 0, text41, 3), contentContainer));
    });

    test('maps translated text by its block', () => {
      // Translation replaces the text nodes but keeps the block element.
      const translated = document.createTextNode('Bonjour le monde');
      block10.replaceChildren(translated);

      assertDeepEquals(
          {anchorNodeId: 11, anchorOffset: 0, focusNodeId: 13, focusOffset: 5},
          mapper.getAxSelection(
              select(translated, 0, translated, 7), contentContainer));
    });
  });

  suite('getReadingModeSelection', () => {
    test('selects the whole block with the selection', () => {
      const result = mapper.getReadingModeSelection(
          selectIds(11, 1, 11, 3), contentContainer);

      assertSelection(select(contentContainer, 0, contentContainer, 1), result);
      // The first block is the anchor's child at the anchor offset.
      assertTrue(!!result);
      assertEquals(block10, result.anchorNode.childNodes[result.anchorOffset]);
    });

    test('selects the whole blocks across the selection', () => {
      assertSelection(
          select(contentContainer, 0, contentContainer, 5),
          mapper.getReadingModeSelection(
              selectIds(13, 2, 31, 1), contentContainer));
    });

    test('stays valid when the blocks are translated', () => {
      const result = mapper.getReadingModeSelection(
          selectIds(11, 1, 11, 3), contentContainer);
      assertTrue(!!result);
      const selection = document.getSelection()!;
      selection.setBaseAndExtent(
          result.anchorNode, result.anchorOffset, result.focusNode,
          result.focusOffset);

      block10.replaceChildren(document.createTextNode('Bonjour le monde'));

      assertEquals(contentContainer, selection.anchorNode);
      assertEquals(0, selection.anchorOffset);
      assertEquals(contentContainer, selection.focusNode);
      assertEquals(1, selection.focusOffset);
      assertStringContains(selection.toString(), 'Bonjour le monde');
    });

    test('returns null if no text is selected', () => {
      // From the end of text 11 to the start of text 13.
      assertNull(mapper.getReadingModeSelection(
          selectIds(11, text11.length, 13, 0), contentContainer));
    });

    test('returns null if an endpoint has no node id', () => {
      assertNull(mapper.getReadingModeSelection(
          {anchorOffset: 1, focusNodeId: 11, focusOffset: 3},
          contentContainer));
      assertNull(mapper.getReadingModeSelection(
          {anchorNodeId: 11, anchorOffset: 1, focusOffset: 3},
          contentContainer));
    });

    test('returns null if the text is not in a block', () => {
      assertNull(mapper.getReadingModeSelection(
          selectIds(41, 0, 41, 3), contentContainer));
    });

    test('returns null if a block is not shown', () => {
      block20.remove();

      assertNull(mapper.getReadingModeSelection(
          selectIds(21, 0, 21, 3), contentContainer));
    });
  });

  suite('with TranslatePdf disabled', () => {
    setup(() => {
      contentBrowserProxy.translatePdfEnabled = false;
    });

    test('getAxSelection returns null', () => {
      assertNull(mapper.getAxSelection(
          select(text11, 1, text11, 3), contentContainer));
    });

    test('getReadingModeSelection returns null', () => {
      assertNull(mapper.getReadingModeSelection(
          selectIds(11, 1, 11, 3), contentContainer));
    });
  });

  suite('when the page is not a PDF', () => {
    setup(() => {
      visualBrowserProxy.pdf = false;
    });

    test('getAxSelection returns null', () => {
      assertNull(mapper.getAxSelection(
          select(text11, 1, text11, 3), contentContainer));
    });

    test('getReadingModeSelection returns null', () => {
      assertNull(mapper.getReadingModeSelection(
          selectIds(11, 1, 11, 3), contentContainer));
    });
  });
});
