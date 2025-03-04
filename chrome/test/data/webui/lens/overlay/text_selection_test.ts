// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/selection_overlay.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {UserAction} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens-overlay/selection_overlay.js';
import type {TextLayerBase} from 'chrome-untrusted://lens-overlay/text_layer_base.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {simulateClick, simulateDrag} from '../utils/selection_utils.js';
import {createLine, createParagraph, createText, createWord, getHighlightedNodesForTesting, getWordNodesForTesting} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

function getCenterX(boundingBox: DOMRect): number {
  return boundingBox.left + boundingBox.width / 2;
}

function getCenterY(boundingBox: DOMRect): number {
  return boundingBox.top + boundingBox.height / 2;
}

// Since we are using percent values, the bounding box values returned via
// getBoundingClientRect() might not be exact pixel values. This check rounds
// to ensure they would be rendered the same at the pixel level.
function assertSameRenderedPixel(expected: number, actual: number) {
  assertEquals(
      Math.round(expected), Math.round(actual),
      `Expected ${expected} to equal ${actual} when rounded, but ${
          Math.round(expected)} != ${Math.round(actual)}`);
}

suite('TextSelection', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;
  let callbackRouterRemote: LensPageRemote;
  let metrics: MetricsTracker;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // Remove the extra word margins to make testing easier.
    loadTimeData.overrideValues({
      'verticalTextMarginPx': 0,
      'horizontalTextMarginPx': 0,
      // TODO(crbug.com/398040980): After launching simplified
      // selection, the tests in this file should be removed as text
      // selection will no longer be supported.
      'simplifiedSelectionEnabled': false,
    });

    // Turn off the shimmer. Since the shimmer is resource intensive, turn off
    // to prevent from causing issues in the tests.
    loadTimeData.overrideValues({'enableShimmer': false});

    selectionOverlayElement = document.createElement('lens-selection-overlay');
    document.body.appendChild(selectionOverlayElement);
    // Since the size of the Selection Overlay is based on the screenshot which
    // is not loaded in the test, we need to force the overlay to take up the
    // viewport.
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';
    // Resize observer does not trigger with flushTasks(), so we need to use
    // waitAfterNextRender() instead.
    await waitAfterNextRender(selectionOverlayElement);

    metrics = fakeMetricsPrivate();
    await flushTasks();
    await addWords();
  });

  function getTextSelectionLayer(): TextLayerBase {
    return selectionOverlayElement.getTextSelectionLayerForTesting();
  }

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
    const formula = createWord(  // X from 5 to 35, Y from 210 to 220.
        '(x + 2) / 4 = 4',
        normalizedBox({x: 20, y: 215, width: 30, height: 10}));
    formula.formulaMetadata = {latex: '\\frac{x + 2}{4} = 4'};

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
      createParagraph([
        createLine([formula]),
      ]),
    ]);
    callbackRouterRemote.textReceived(text);
    await flushTasks();
    await waitAfterNextRender(selectionOverlayElement);
  }

  function getRenderedWords(): NodeListOf<Element> {
    return getWordNodesForTesting(
        getTextSelectionLayer().getElementForTesting());
  }

  function getHighlightedLines(): NodeListOf<Element> {
    return getHighlightedNodesForTesting(
        getTextSelectionLayer().getElementForTesting());
  }

  test('verify that text renders on the page', () => {
    const wordsOnPage = getRenderedWords();

    assertEquals(9, wordsOnPage.length);
  });

  test('verify that dragging over a word highlights the word', async () => {
    const wordsOnPage = getRenderedWords();
    const firstWordBoundingBox = wordsOnPage[0]!.getBoundingClientRect();

    // Drag from beginning of first word to end of first word.
    await simulateDrag(
        selectionOverlayElement,
        {x: firstWordBoundingBox.left + 2, y: firstWordBoundingBox.top + 2},
        {x: firstWordBoundingBox.right - 2, y: firstWordBoundingBox.top + 2});

    const highlightedLines = getHighlightedLines();
    assertEquals(1, highlightedLines.length);

    assertSameRenderedPixel(
        firstWordBoundingBox.left,
        highlightedLines[0]!.getBoundingClientRect().left);
    assertSameRenderedPixel(
        firstWordBoundingBox.top,
        highlightedLines[0]!.getBoundingClientRect().top);

    // Verify the correct request was made.
    const textQuery =
        await testBrowserProxy.handler.whenCalled('issueTextSelectionRequest');
    assertEquals(1, metrics.count('Lens.Overlay.Overlay.UserAction'));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction', UserAction.kTextSelection));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kTextSelection));
    assertEquals('hello', textQuery);
    const action = await testBrowserProxy.handler.whenCalled(
        'recordUkmAndTaskCompletionForLensOverlayInteraction');
    assertEquals(UserAction.kTextSelection, action);
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
        await waitAfterNextRender(selectionOverlayElement);

        const highlightedLines = getHighlightedLines();
        assertEquals(1, highlightedLines.length);

        const highlightedLineBoundingBox =
            highlightedLines[0]!.getBoundingClientRect();
        assertSameRenderedPixel(
            firstWordBoundingBox.left, highlightedLineBoundingBox.left);
        assertSameRenderedPixel(
            firstWordBoundingBox.top, highlightedLineBoundingBox.top);
        assertSameRenderedPixel(
            secondWordBoundingBox.right, highlightedLineBoundingBox.right);
        assertSameRenderedPixel(
            secondWordBoundingBox.bottom, highlightedLineBoundingBox.bottom);

        // Verify the correct request was made.
        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTextSelectionRequest');
        assertEquals('hello there', textQuery);
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
              x: thirdWordBoundingBox.right + 5,
              y: thirdWordBoundingBox.top - 5,
            });

        const highlightedLines = getHighlightedLines();
        assertEquals(1, highlightedLines.length);

        const highlightedLineBoundingBox =
            highlightedLines[0]!.getBoundingClientRect();
        assertSameRenderedPixel(
            secondWordBoundingBox.left, highlightedLineBoundingBox.left);
        assertSameRenderedPixel(
            secondWordBoundingBox.top, highlightedLineBoundingBox.top);
        assertSameRenderedPixel(
            thirdWordBoundingBox.right, highlightedLineBoundingBox.right);
        assertSameRenderedPixel(
            thirdWordBoundingBox.bottom, highlightedLineBoundingBox.bottom);

        // Verify the correct request was made.
        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTextSelectionRequest');
        assertEquals('there test', textQuery);
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
        await waitAfterNextRender(selectionOverlayElement);

        const highlightedLines = getHighlightedLines();
        assertEquals(2, highlightedLines.length);

        // Verify the correct request was made.
        const textQuery = await testBrowserProxy.handler.whenCalled(
            'issueTextSelectionRequest');
        assertEquals('hello there test a new line', textQuery);
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

    const highlightedLines = getHighlightedLines();
    assertEquals(2, highlightedLines.length);

    assertSameRenderedPixel(
        firstParagraphLastWordBox.left,
        highlightedLines[0]!.getBoundingClientRect().left);
    assertSameRenderedPixel(
        firstParagraphLastWordBox.top,
        highlightedLines[0]!.getBoundingClientRect().top);
    assertSameRenderedPixel(
        secondParagraphSecondWordBox.right,
        highlightedLines[1]!.getBoundingClientRect().right);
    assertSameRenderedPixel(
        secondParagraphSecondWordBox.bottom,
        highlightedLines[1]!.getBoundingClientRect().bottom);

    // Verify the correct request was made.
    const textQuery =
        await testBrowserProxy.handler.whenCalled('issueTextSelectionRequest');
    assertEquals('line FAKE HEADING', textQuery);
  });

  test('verify that selecting formula works', async () => {
    const wordsOnPage = getRenderedWords();
    const formulaBoundingBox = wordsOnPage[8]!.getBoundingClientRect();

    // Drag from beginning of first word to end of first word.
    await simulateDrag(
        selectionOverlayElement,
        {x: formulaBoundingBox.left + 2, y: formulaBoundingBox.top + 2},
        {x: formulaBoundingBox.right - 2, y: formulaBoundingBox.top + 2});

    const highlightedLines = getHighlightedLines();
    assertEquals(1, highlightedLines.length);

    assertSameRenderedPixel(
        formulaBoundingBox.left,
        highlightedLines[0]!.getBoundingClientRect().left);
    assertSameRenderedPixel(
        formulaBoundingBox.top,
        highlightedLines[0]!.getBoundingClientRect().top);

    // Verify the correct request was made.
    const textQuery =
        await testBrowserProxy.handler.whenCalled('issueMathSelectionRequest');
    assertEquals(1, metrics.count('Lens.Overlay.Overlay.UserAction'));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction', UserAction.kMathSelection));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kMathSelection));
    assertEquals('(x + 2) / 4 = 4', textQuery[0]);
    assertEquals('\\frac{x + 2}{4} = 4', textQuery[1]);
    const action = await testBrowserProxy.handler.whenCalled(
        'recordUkmAndTaskCompletionForLensOverlayInteraction');
    assertEquals(UserAction.kMathSelection, action);
  });

  test('verify that starting a drag off a word does nothing', async () => {
    await simulateDrag(selectionOverlayElement, {x: 0, y: 0}, {x: 70, y: 35});

    const highlightedLines = getHighlightedLines();

    assertEquals(0, highlightedLines.length);
    assertEquals(
        0, testBrowserProxy.handler.getCallCount('issueTextSelectionRequest'));
    assertEquals(1, metrics.count('Lens.Overlay.Overlay.UserAction'));
    assertEquals(
        1,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction'));
    assertEquals(
        0,
        metrics.count(
            'Lens.Overlay.Overlay.UserAction', UserAction.kTextSelection));
    assertEquals(
        0,
        metrics.count(
            'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
            UserAction.kTextSelection));
    const action = await testBrowserProxy.handler.whenCalled(
        'recordUkmAndTaskCompletionForLensOverlayInteraction');
    assertEquals(UserAction.kRegionSelection, action);
  });

  test(
    'verify that clicking on a word highlights the clicked word',
    async () => {
      const wordsOnPage = getRenderedWords();
      const word = wordsOnPage[0]!.getBoundingClientRect();

      let highlightedLines = getHighlightedLines();
      assertEquals(0, highlightedLines.length);

      // Click on a word.
      await simulateClick(
          selectionOverlayElement,
          {x: getCenterX(word), y: getCenterY(word)});

      // Verify words unhighlight, except for the tapped word.
      highlightedLines = getHighlightedLines();
      assertEquals(1, highlightedLines.length);

      const highlightedLineBoundingBox =
          highlightedLines[0]!.getBoundingClientRect();
      assertSameRenderedPixel(
        word.left, highlightedLineBoundingBox.left);
      assertSameRenderedPixel(
        word.top, highlightedLineBoundingBox.top);
      assertSameRenderedPixel(
        word.right, highlightedLineBoundingBox.right);
      assertSameRenderedPixel(
        word.bottom, highlightedLineBoundingBox.bottom);
    });

  test(
    `verify that clicking on a word unhighlights words except
    for the clicked word`,
    async () => {
      const wordsOnPage = getRenderedWords();
      const firstWord = wordsOnPage[0]!.getBoundingClientRect();
      const secondWord = wordsOnPage[1]!.getBoundingClientRect();

      // Highlight some words.
      await simulateDrag(
          selectionOverlayElement,
          {x: getCenterX(firstWord), y: getCenterY(firstWord)},
          {x: getCenterX(secondWord), y: getCenterY(secondWord)});
      let highlightedLines = getHighlightedLines();
      assertEquals(1, highlightedLines.length);

      let highlightedLineBoundingBox =
          highlightedLines[0]!.getBoundingClientRect();
      assertSameRenderedPixel(
        firstWord.left, highlightedLineBoundingBox.left);
      assertSameRenderedPixel(
        firstWord.top, highlightedLineBoundingBox.top);
      assertSameRenderedPixel(
        secondWord.right, highlightedLineBoundingBox.right);
      assertSameRenderedPixel(
        secondWord.bottom, highlightedLineBoundingBox.bottom);

      // Click on a word.
      await simulateClick(
          selectionOverlayElement,
          {x: getCenterX(firstWord), y: getCenterY(firstWord)});

      // Verify words unhighlight, except for the clicked word.
      highlightedLines = getHighlightedLines();
      assertEquals(1, highlightedLines.length);

      highlightedLineBoundingBox =
          highlightedLines[0]!.getBoundingClientRect();
      assertEquals(1, highlightedLines.length);
      assertSameRenderedPixel(
        firstWord.left, highlightedLineBoundingBox.left);
      assertSameRenderedPixel(
        firstWord.top, highlightedLineBoundingBox.top);
      assertSameRenderedPixel(
        firstWord.right, highlightedLineBoundingBox.right);
      assertSameRenderedPixel(
        firstWord.bottom, highlightedLineBoundingBox.bottom);
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
    let highlightedLines = getHighlightedLines();
    assertEquals(1, highlightedLines.length);

    // Click off a word.
    await simulateClick(selectionOverlayElement, {x: 0, y: 0});

    // Verify words unhighlight.
    highlightedLines = getHighlightedLines();
    assertEquals(0, highlightedLines.length);
  });

  test('TextLayerClearAllSelectionsCallback', async () => {
    const wordsOnPage = getRenderedWords();
    const firstWord = wordsOnPage[0]!.getBoundingClientRect();
    const secondWord = wordsOnPage[1]!.getBoundingClientRect();

    // Highlight some words.
    await simulateDrag(
        selectionOverlayElement,
        {x: getCenterX(firstWord), y: getCenterY(firstWord)},
        {x: getCenterX(secondWord), y: getCenterY(secondWord)});
    let highlightedLines = getHighlightedLines();
    assertEquals(1, highlightedLines.length);

    // Mojo call to clear all selections.
    callbackRouterRemote.clearAllSelections();
    await flushTasks();

    // Verify words unhighlight.
    highlightedLines = getHighlightedLines();
    assertEquals(0, highlightedLines.length);
  });

  test('TextLayerSetTextSelectionCallback', async () => {
    // Verify no words are highlighted.
    let highlightedLines = getHighlightedLines();
    assertEquals(0, highlightedLines.length);

    // Mojo call to clear all selections.
    callbackRouterRemote.setTextSelection(0, 1);
    await flushTasks();

    // Verify words are now highlighted.
    highlightedLines = getHighlightedLines();
    assertEquals(1, highlightedLines.length);
  });

  // TODO(b/336797761): Add tests that test rotated bounding boxes and top to
  // bottom writing directions.
});
