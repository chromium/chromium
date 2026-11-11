// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BLOCK_HIGHLIGHT_SHOWN_CLASS, BLOCK_SELECTION_HIGHLIGHT_NAME} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {BlockSelectionHighlighter} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setupTestEnvironment} from './common.js';
import type {TestContentBrowserProxy} from './test_content_browser_proxy.js';

suite('BlockSelectionHighlighter', () => {
  let highlighter: BlockSelectionHighlighter;
  let contentBrowserProxy: TestContentBrowserProxy;

  // <div>        contentContainer
  //   <p>One</p>
  //   <p>Two</p>
  // </div>
  let contentContainer: HTMLElement;

  // Returns the forward selection of the blocks from start to end - 1.
  function selectBlocks(container: Node, start: number, end: number) {
    return {
      anchorNode: container,
      anchorOffset: start,
      focusNode: container,
      focusOffset: end,
    };
  }

  function assertHighlightedRange(
      container: HTMLElement, startOffset: number, endOffset: number) {
    const highlight = CSS.highlights.get(BLOCK_SELECTION_HIGHLIGHT_NAME);
    assertTrue(!!highlight);
    const ranges = Array.from(highlight);
    assertEquals(1, ranges.length);
    const range = ranges[0]!;
    assertEquals(container, range.startContainer);
    assertEquals(startOffset, range.startOffset);
    assertEquals(container, range.endContainer);
    assertEquals(endOffset, range.endOffset);
    assertTrue(container.classList.contains(BLOCK_HIGHLIGHT_SHOWN_CLASS));
  }

  function createContainer(): HTMLElement {
    const container = document.createElement('div');
    for (const text of ['One', 'Two']) {
      const block = document.createElement('p');
      block.textContent = text;
      container.appendChild(block);
    }
    document.body.appendChild(container);
    return container;
  }

  setup(() => {
    const result = setupTestEnvironment();
    highlighter = result.blockSelectionHighlighter;
    contentBrowserProxy = result.contentBrowserProxy;
    contentBrowserProxy.translatePdfEnabled = true;
    CSS.highlights.clear();
    contentContainer = createContainer();
  });

  teardown(() => {
    CSS.highlights.clear();
  });

  test('highlights the blocks', () => {
    highlighter.highlight(
        selectBlocks(contentContainer, 0, 2), contentContainer);

    assertHighlightedRange(contentContainer, 0, 2);
  });

  test('replaces the previous highlight', () => {
    highlighter.highlight(
        selectBlocks(contentContainer, 0, 2), contentContainer);

    highlighter.highlight(
        selectBlocks(contentContainer, 1, 2), contentContainer);

    assertHighlightedRange(contentContainer, 1, 2);
  });

  test('does not replace the highlight for the same blocks', () => {
    highlighter.highlight(
        selectBlocks(contentContainer, 0, 1), contentContainer);
    const highlight = CSS.highlights.get(BLOCK_SELECTION_HIGHLIGHT_NAME);

    highlighter.highlight(
        selectBlocks(contentContainer, 0, 1), contentContainer);

    assertEquals(
        highlight, CSS.highlights.get(BLOCK_SELECTION_HIGHLIGHT_NAME));
  });

  test('moves the class to a new content container', () => {
    const otherContainer = createContainer();
    highlighter.highlight(
        selectBlocks(contentContainer, 0, 1), contentContainer);

    highlighter.highlight(selectBlocks(otherContainer, 0, 1), otherContainer);

    assertHighlightedRange(otherContainer, 0, 1);
    assertFalse(
        contentContainer.classList.contains(BLOCK_HIGHLIGHT_SHOWN_CLASS));
  });

  test('clear removes the highlight', () => {
    highlighter.highlight(
        selectBlocks(contentContainer, 0, 2), contentContainer);

    highlighter.clear();

    assertFalse(CSS.highlights.has(BLOCK_SELECTION_HIGHLIGHT_NAME));
    assertFalse(
        contentContainer.classList.contains(BLOCK_HIGHLIGHT_SHOWN_CLASS));
  });

  test('clear does nothing without a highlight', () => {
    highlighter.clear();

    assertFalse(CSS.highlights.has(BLOCK_SELECTION_HIGHLIGHT_NAME));
  });

  test('does nothing if disabled', () => {
    contentBrowserProxy.translatePdfEnabled = false;

    highlighter.highlight(
        selectBlocks(contentContainer, 0, 2), contentContainer);

    assertFalse(CSS.highlights.has(BLOCK_SELECTION_HIGHLIGHT_NAME));
    assertFalse(
        contentContainer.classList.contains(BLOCK_HIGHLIGHT_SHOWN_CLASS));
  });
});
