// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/selection_overlay.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import type {LensPageRemote} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens/selection_overlay.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {simulateClick, simulateDrag} from '../utils/selection_utils.js';
import {createLine, createParagraph, createText, createWord} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

function getCenterX(boundingBox: DOMRect): number {
  return boundingBox.left + boundingBox.width / 2;
}

function getCenterY(boundingBox: DOMRect): number {
  return boundingBox.top + boundingBox.height / 2;
}

suite('TextSelection', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;
  let callbackRouterRemote: LensPageRemote;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    selectionOverlayElement = document.createElement('lens-selection-overlay');
    document.body.appendChild(selectionOverlayElement);
    // Since the size of the Selection Overlay is based on the screenshot which
    // is not loaded in the test, we need to force the overlay to take up the
    // viewport.
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';
    await flushTasks();
    await addWords();
  });

  // Normalizes the given values to the size of selection overlay.
  function normalizedBox(box: RectF): RectF {
    const boundingRect = selectionOverlayElement.getBoundingClientRect();
    return {
      x: box.x / boundingRect.width,
      y: box.y / boundingRect.height,
      width: box.width / boundingRect.width,
      height: box.height / boundingRect.height,
    };
  }

  async function addWords() {
    const text = createText([
      createParagraph([
        createLine([
          createWord(  // X from 5 to 35, Y from 10 to 20.
              'hello', normalizedBox({x: 20, y: 15, width: 30, height: 10})),
          createWord(  // X from 45 to 95, Y from 10 to 20.
              'there', normalizedBox({x: 70, y: 15, width: 50, height: 10})),
          createWord(  // X from 105 to 135, Y from 10 to 20.
              'test', normalizedBox({x: 120, y: 15, width: 30, height: 10})),
        ]),
        createLine([
          createWord(  // X from 8 to 20, Y from 30 to 40.
              'a', normalizedBox({x: 14, y: 35, width: 12, height: 10})),
          createWord(  // X from 30 to 50, Y from 30 to 40.
              'new', normalizedBox({x: 40, y: 35, width: 20, height: 10})),
          createWord(  // X from 60 to 80, Y from 30 to 40.
              'line', normalizedBox({x: 70, y: 35, width: 20, height: 10})),
        ]),
      ]),
      createParagraph([
        createLine([
          createWord(  // X from 200 to 260, Y from 35 to 65.
              'FAKE', normalizedBox({x: 230, y: 50, width: 60, height: 30})),
          createWord(  // X from 280 to 360, Y from 35 to 65.
              'HEADING', normalizedBox({x: 320, y: 50, width: 80, height: 30})),
        ]),
      ]),
    ]);
    callbackRouterRemote.textReceived(text);
    return flushTasks();
  }

  function getRenderedWords(): NodeListOf<Element> {
    return selectionOverlayElement.$.textSelectionLayer
        .getWordNodesForTesting();
  }

  function getHighlightedWords(): NodeListOf<Element> {
    return selectionOverlayElement.$.textSelectionLayer
        .getHighlightedNodesForTesting();
  }

  test('verify that text renders on the page', async () => {
    const wordsOnPage = getRenderedWords();

    assertEquals(8, wordsOnPage.length);
  });

  test('verify that dragging over a word highlights the word', async () => {
    const wordsOnPage = getRenderedWords();
    const firstWordBoundingBox = wordsOnPage[0]!.getBoundingClientRect();

    // Drag from beginning of first word to end of first word.
    await simulateDrag(
        selectionOverlayElement,
        {x: firstWordBoundingBox.left + 2, y: firstWordBoundingBox.top + 2},
        {x: firstWordBoundingBox.right - 2, y: firstWordBoundingBox.top + 2});

    const highlightedWords = getHighlightedWords();

    assertEquals(1, highlightedWords.length);
    assertEquals(
        firstWordBoundingBox.x, highlightedWords[0]!.getBoundingClientRect().x);
    assertEquals(
        firstWordBoundingBox.y, highlightedWords[0]!.getBoundingClientRect().y);
  });

  test(
      'verify that dragging over two words highlights both words', async () => {
        const wordsOnPage = getRenderedWords();
        const firstWordBoundingBox = wordsOnPage[0]!.getBoundingClientRect();
        const secondWordBoundingBox = wordsOnPage[1]!.getBoundingClientRect();

        // Drag from first word onto second word.
        await simulateDrag(
            selectionOverlayElement, {
              x: getCenterX(firstWordBoundingBox),
              y: getCenterY(firstWordBoundingBox),
            },
            {
              x: getCenterX(secondWordBoundingBox),
              y: getCenterY(secondWordBoundingBox),
            });

        const highlightedWords = getHighlightedWords();

        assertEquals(2, highlightedWords.length);
        assertEquals(
            firstWordBoundingBox.x,
            highlightedWords[0]!.getBoundingClientRect().x);
        assertEquals(
            firstWordBoundingBox.y,
            highlightedWords[0]!.getBoundingClientRect().y);
        assertEquals(
            secondWordBoundingBox.x,
            highlightedWords[1]!.getBoundingClientRect().x);
        assertEquals(
            secondWordBoundingBox.y,
            highlightedWords[1]!.getBoundingClientRect().y);
      });

  test(
      'verify that dragging off a word highlights the closest word',
      async () => {
        const wordsOnPage = getRenderedWords();
        const secondWordBoundingBox = wordsOnPage[1]!.getBoundingClientRect();
        const thirdWordBoundingBox = wordsOnPage[2]!.getBoundingClientRect();

        // Drag from second word to off line and above third word.
        await simulateDrag(
            selectionOverlayElement, {
              x: getCenterX(secondWordBoundingBox),
              y: getCenterY(secondWordBoundingBox),
            },
            {
              x: getCenterX(thirdWordBoundingBox),
              y: getCenterY(thirdWordBoundingBox),
            });

        const highlightedWords = getHighlightedWords();

        assertEquals(2, highlightedWords.length);
        assertEquals(
            secondWordBoundingBox.x,
            highlightedWords[0]!.getBoundingClientRect().x);
        assertEquals(
            secondWordBoundingBox.y,
            highlightedWords[0]!.getBoundingClientRect().y);
        assertEquals(
            thirdWordBoundingBox.x,
            highlightedWords[1]!.getBoundingClientRect().x);
        assertEquals(
            thirdWordBoundingBox.y,
            highlightedWords[1]!.getBoundingClientRect().y);
      });

  test(
      `verify that dragging from the start of a paragraph to the end highlights
      all words in the paragraph`,
      async () => {
        const wordsOnPage = getRenderedWords();
        const firstParagraphFirstWordBox =
            wordsOnPage[0]!.getBoundingClientRect();
        const firstParagraphLastWordBox =
            wordsOnPage[5]!.getBoundingClientRect();


        // Drag from first word to the right of last word of paragraph.
        await simulateDrag(
            selectionOverlayElement, {
              x: getCenterX(firstParagraphFirstWordBox),
              y: getCenterY(firstParagraphFirstWordBox),
            },
            {
              x: firstParagraphLastWordBox.right + 2,
              y: getCenterY(firstParagraphLastWordBox),
            });

        const highlightedWords = getHighlightedWords();

        assertEquals(6, highlightedWords.length);
      });

  test('verify that dragging across paragraphs works', async () => {
    const wordsOnPage = getRenderedWords();
    const firstParagraphLastWordBox = wordsOnPage[5]!.getBoundingClientRect();
    const secondParagraphSecondWordBox =
        wordsOnPage[7]!.getBoundingClientRect();

    // Drag from last word of first paragraph to second word of second
    // paragraph.
    await simulateDrag(
        selectionOverlayElement, {
          x: getCenterX(firstParagraphLastWordBox),
          y: getCenterY(firstParagraphLastWordBox),
        },
        {
          x: getCenterX(secondParagraphSecondWordBox),
          y: getCenterY(secondParagraphSecondWordBox),
        });

    const highlightedWords = getHighlightedWords();

    assertEquals(3, highlightedWords.length);

    assertEquals(
        firstParagraphLastWordBox.x,
        highlightedWords[0]!.getBoundingClientRect().x);
    assertEquals(
        firstParagraphLastWordBox.y,
        highlightedWords[0]!.getBoundingClientRect().y);
    assertEquals(
        secondParagraphSecondWordBox.x,
        highlightedWords[2]!.getBoundingClientRect().x);
    assertEquals(
        secondParagraphSecondWordBox.y,
        highlightedWords[2]!.getBoundingClientRect().y);
  });

  test('verify that starting a drag off a word does nothing', async () => {
    await simulateDrag(selectionOverlayElement, {x: 0, y: 0}, {x: 70, y: 35});

    const highlightedWords = getHighlightedWords();

    assertEquals(0, highlightedWords.length);
  });

  test('verify that clicking on a word unhighlights words', async () => {
    const wordsOnPage = getRenderedWords();
    const firstWord = wordsOnPage[0]!.getBoundingClientRect();
    const secondWord = wordsOnPage[1]!.getBoundingClientRect();

    // Highlight some words.
    await simulateDrag(
        selectionOverlayElement,
        {x: getCenterX(firstWord), y: getCenterY(firstWord)},
        {x: getCenterX(secondWord), y: getCenterY(secondWord)});
    let highlightedWords = getHighlightedWords();
    assertEquals(2, highlightedWords.length);

    // Click on a word.
    await simulateClick(
        selectionOverlayElement,
        {x: getCenterX(firstWord), y: getCenterY(firstWord)});

    // Verify words unhighlight.
    highlightedWords = getHighlightedWords();
    assertEquals(0, highlightedWords.length);
  });

  test('verify that clicking off a word unhighlights words', async () => {
    const wordsOnPage = getRenderedWords();
    const firstWord = wordsOnPage[0]!.getBoundingClientRect();
    const secondWord = wordsOnPage[1]!.getBoundingClientRect();

    // Highlight some words.
    await simulateDrag(
        selectionOverlayElement,
        {x: getCenterX(firstWord), y: getCenterY(firstWord)},
        {x: getCenterX(secondWord), y: getCenterY(secondWord)});
    let highlightedWords = getHighlightedWords();
    assertEquals(2, highlightedWords.length);

    // Click off a word.
    await simulateClick(selectionOverlayElement, {x: 0, y: 0});

    // Verify words unhighlight.
    highlightedWords = getHighlightedWords();
    assertEquals(0, highlightedWords.length);
  });

  // TODO(b/328294794): Once there is logic for doing something after a text
  // selection, add tests to ensure finishing a drag off the selection overlay
  // still triggers text selection logic.
});
