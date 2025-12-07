// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://lens-overlay/selection_overlay.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {SemanticEvent} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {SimplifiedTextLayerElement} from 'chrome-untrusted://lens-overlay/simplified_text_layer.js';
import {WritingDirection} from 'chrome-untrusted://lens-overlay/text.mojom-webui.js';
import type {TextCopyCallback} from 'chrome-untrusted://lens-overlay/text_layer_base.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome-untrusted://webui-test/test_util.js';

import {fakeScreenshotBitmap} from '../utils/image_utils.js';
import {assertWithinThreshold} from '../utils/object_utils.js';
import {addEmptyRegionTextToPage, addEmptyTextToPage, addGenericRegionWordsToPageNormalized, addGenericWordsToPageNormalized, addRegionTextToPage, createLine, createParagraph, createText, createWord} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

const TEXT_RECEIVED_TIMEOUT_MS = 1000000;
const COPY_TEXT_TIMEOUT_MS = 1000001;
const TRANSLATE_TEXT_TIMEOUT_MS = 1000002;

const CURVED_TEXT = createText([
  createParagraph([
    createLine([
      createWord(
          'hello', {x: 0.1, y: 0.1, width: 0.1, height: 0.1},
          /*rotation=*/ 0.1),
      createWord(
          'there', {x: 0.11, y: 0.11, width: 0.1, height: 0.1},
          /*rotation=*/ 0.101),
    ]),
  ]),
  createParagraph([
    createLine([createWord(
        'world', {x: 0.3, y: 0.3, width: 0.1, height: 0.1},
        /*rotation=*/ 0.2)]),
  ]),
]);
const TOP_TO_BOTTOM_TEXT = createText([
  createParagraph([
    createLine([
      createWord(
          'hello', {x: 0.1, y: 0.1, width: 0.1, height: 0.11}, /*rotation=*/ 0,
          WritingDirection.kTopToBottom),
      createWord(
          'there', {x: 0.11, y: 0.11, width: 0.12, height: 0.1},
          /*rotation=*/ 0, WritingDirection.kTopToBottom),
    ]),
  ]),
  createParagraph([
    createLine([createWord(
        'world', {x: 0.3, y: 0.3, width: 0.1, height: 0.11}, /*rotation=*/ 0,
        WritingDirection.kTopToBottom)]),
  ]),
]);

suite('SimplifiedSelection', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let textLayerElement: SimplifiedTextLayerElement;
  let callbackRouterRemote: LensPageRemote;
  let textReceivedTimeoutFunction: Function|undefined;
  let copyTextTimeoutFunction: Function|undefined;
  let translateTextTimeoutFunction: Function|undefined;
  let selectionOverlayRect: DOMRect;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      'textReceivedTimeout': TEXT_RECEIVED_TIMEOUT_MS,
      'copyTextTimeout': COPY_TEXT_TIMEOUT_MS,
      'translateTextTimeout': TRANSLATE_TEXT_TIMEOUT_MS,
    });

    // Override setTimeout, and only alter behavior for the text received
    // timeout. Using MockTimer did not work here, as it interfered with many
    // other, unrelated timers causing tests to crash.
    const origSetTimeout = window.setTimeout;
    window.setTimeout = function(
        handler: TimerHandler, timeout: number|undefined): number {
      if (timeout === TEXT_RECEIVED_TIMEOUT_MS) {
        const callback = handler as Function;
        textReceivedTimeoutFunction = callback;
        return 0;
      }
      if (timeout === COPY_TEXT_TIMEOUT_MS) {
        const callback = handler as Function;
        copyTextTimeoutFunction = callback;
        return 0;
      }
      if (timeout === TRANSLATE_TEXT_TIMEOUT_MS) {
        const callback = handler as Function;
        translateTextTimeoutFunction = callback;
        return 0;
      }
      return origSetTimeout(handler, timeout);
    };

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    textLayerElement = document.createElement('lens-simplified-text-layer');
    selectionOverlayRect = new DOMRect(0, 0, 100, 100);
    textLayerElement.setSelectionOverlayRectForTesting(selectionOverlayRect);
    document.body.appendChild(textLayerElement);
    await waitAfterNextRender(textLayerElement);
  });

  teardown(() => {
    textReceivedTimeoutFunction = undefined;
    copyTextTimeoutFunction = undefined;
    translateTextTimeoutFunction = undefined;
  });

  function callTextReceivedTimeout() {
    assertTrue(textReceivedTimeoutFunction !== undefined);
    textReceivedTimeoutFunction();
  }

  function callCopyTextTimeout() {
    assertTrue(copyTextTimeoutFunction !== undefined);
    copyTextTimeoutFunction();
  }

  function callTranslateTextTimeout() {
    assertTrue(translateTextTimeoutFunction !== undefined);
    translateTextTimeoutFunction();
  }

  async function dispatchDetectTextInRegionEvent() {
    const centerRotatedBox = {
      box: {x: 0.2, y: 0.2, width: 0.4, height: 0.4},
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
    textLayerElement.dispatchEvent(
        new CustomEvent<CenterRotatedBox>('detect-text-in-region', {
          bubbles: true,
          composed: true,
          detail: centerRotatedBox,
        }));
    await flushTasks();
  }

  test('OnSelectionStartFiresHideContextMenuEvent', async () => {
    textLayerElement.onSelectionStart();

    const hideSelectedRegionContextMenuEventPromise =
        eventToPromise('hide-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    await hideSelectedRegionContextMenuEventPromise;
  });

  test('OnSelectionFinishedClearsText', async () => {
    const receivedTextEventPromise =
        eventToPromise('finished-receiving-text', document.body);
    await addEmptyTextToPage(callbackRouterRemote);
    await receivedTextEventPromise;
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);

    // Simulate a new selection being created.
    textLayerElement.onSelectionStart();
    textLayerElement.onSelectionFinish();

    // When the detect text in region event is received, the context menu should
    // be shown without any detected text.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);

    await dispatchDetectTextInRegionEvent();

    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, -1);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, -1);
  });

  test('InjectedImageUsesFullTextForHighlights', async () => {
    // Set the full text response.
    await addGenericWordsToPageNormalized(callbackRouterRemote);

    // Set the selected region to contain "hello" and "there".
    const selectedRegion: CenterRotatedBox = {
      box: {x: 0.1, y: 0.1, width: 0.15, height: 0.15},
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
    callbackRouterRemote.setPostRegionSelection(selectedRegion);
    await flushTasks();

    // Send region text for an injected image. The words in this response
    // should be ignored for highlighting, but will be stored as the region
    // text.
    await addEmptyRegionTextToPage(
        callbackRouterRemote, /*isInjectedImage=*/ true);
    await waitAfterNextRender(textLayerElement);

    // Verify the highlighted lines are from the full text response, not the
    // injected region text response.
    const highlightedLineElements: NodeListOf<Element> =
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line');
    assertEquals(1, highlightedLineElements.length);
  });

  test('FullTextAfterRegionTextForInjectedImage', async () => {
    // Set the selected region to contain "hello" and "there".
    const selectedRegion: CenterRotatedBox = {
      box: {x: 0.1, y: 0.1, width: 0.15, height: 0.15},
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
    callbackRouterRemote.setPostRegionSelection(selectedRegion);
    await flushTasks();

    // Send region text for an injected image, but with no words. This
    // simulates the region text response arriving before the full text
    // response.
    await addEmptyRegionTextToPage(
        callbackRouterRemote, /*isInjectedImage=*/ true);
    await waitAfterNextRender(textLayerElement);

    // No highlights should be rendered yet.
    let highlightedLineElements: NodeListOf<Element> =
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line');
    assertEquals(0, highlightedLineElements.length);

    // Now, send the full text response.
    await addGenericWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);

    // Highlights should now be rendered based on the full text response.
    highlightedLineElements =
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line');
    assertEquals(1, highlightedLineElements.length);
  });

  test('RegionTextReceivedLogsSemanticEvents', async () => {
    // Set up empty full text response.
    await addEmptyTextToPage(callbackRouterRemote);
    let semanticEventArgs = await testBrowserProxy.handler.getArgs(
        'recordLensOverlaySemanticEvent');
    // No semantic events should be logged yet.
    assertEquals(0, semanticEventArgs.length);

    // Add region text. This should log a start event.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    assertEquals(1, semanticEventArgs.length);
    assertEquals(SemanticEvent.kTextGleamsViewStart, semanticEventArgs[0]);

    // Add new region text. This should log an end event for the previous
    // text, and a start event for the new text.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    semanticEventArgs = await testBrowserProxy.handler.getArgs(
        'recordLensOverlaySemanticEvent');
    assertEquals(3, semanticEventArgs.length);
    assertEquals(SemanticEvent.kTextGleamsViewEnd, semanticEventArgs[1]);
    assertEquals(SemanticEvent.kTextGleamsViewStart, semanticEventArgs[2]);
  });

  test('HasActionedTextResetsAfterNewSelection', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    assertFalse(textLayerElement.getHasActionedTextForTesting());

    // Simulate a new selection being created.
    textLayerElement.onSelectionStart();
    textLayerElement.onSelectionFinish();
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);

    // Simulate an action.
    textLayerElement.onCopyDetectedText(/*startIndex=*/ 0,
                                        /*endIndex=*/ 2,
                                        /*callback=*/ () => {});
    assertTrue(textLayerElement.getHasActionedTextForTesting());

    // Simulate another selection being created.
    textLayerElement.onSelectionStart();
    assertFalse(textLayerElement.getHasActionedTextForTesting());
    textLayerElement.onSelectionFinish();
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);

    // Simulate an action.
    textLayerElement.selectAndTranslateWords(/*startIndex=*/ 0,
                                             /*endIndex=*/ 2);
    assertTrue(textLayerElement.getHasActionedTextForTesting());

    // Simulate another selection being created.
    textLayerElement.onSelectionStart();
    assertFalse(textLayerElement.getHasActionedTextForTesting());
    textLayerElement.onSelectionFinish();
  });

  test('HideContextMenuTimeoutOngoingNoText', async () => {
    const hideSelectedRegionContextMenuEventPromise =
        eventToPromise('hide-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    // If the timeout has not elapsed, the selected region context menu will be
    // called to be hidden instead.
    await hideSelectedRegionContextMenuEventPromise;

    // Since there was no text, there should be no call to record a text gleam.
    assertEquals(
        0,
        testBrowserProxy.handler.getCallCount(
            'recordLensOverlaySemanticEvent'));
  });

  test(
      'SelectedRegionContextMenuAppearsAfterTimeoutElapsesNoText', async () => {
        callTextReceivedTimeout();

        // When the detect text in region event is received, the context menu
        // should be shown without any detected text.
        const showSelectedRegionContextMenuEventPromise =
            eventToPromise('show-selected-region-context-menu', document.body);
        await dispatchDetectTextInRegionEvent();
        const showSelectedRegionContextMenuEvent =
            await showSelectedRegionContextMenuEventPromise;
        assertEquals(
            showSelectedRegionContextMenuEvent.detail.selectionEndIndex, -1);
        assertEquals(
            showSelectedRegionContextMenuEvent.detail.selectionStartIndex, -1);

        // Since there was no text, there should be no call to record a text
        // gleam.
        assertEquals(
            0,
            testBrowserProxy.handler.getCallCount(
                'recordLensOverlaySemanticEvent'));
      });

  test('SelectedRegionContextMenuAppearsWithEmptyText', async () => {
    const receivedTextEventPromise =
        eventToPromise('finished-receiving-text', document.body);

    await addEmptyTextToPage(callbackRouterRemote);
    await receivedTextEventPromise;

    // Since there was no text, there should be no call to record a text gleam.
    assertEquals(
        0,
        testBrowserProxy.handler.getCallCount(
            'recordLensOverlaySemanticEvent'));

    // Simulate a new selection being created.
    textLayerElement.onSelectionStart();
    textLayerElement.onSelectionFinish();

    // When the detect text in region event is received, the context menu should
    // be shown without any detected text.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, -1);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, -1);
  });

  test('SelectedRegionContextMenuAppearsWithFullImageText', async () => {
    await addGenericWordsToPageNormalized(callbackRouterRemote);

    // When the detect text in region event is received, the context menu should
    // be shown without any detected text.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, 0);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, 2);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.text, 'hello there\r\ntest');
  });

  test('SelectedRegionContextMenuAppearsWithRegionText', async () => {
    // Two add text calls to have text be used from the region.
    await addEmptyTextToPage(callbackRouterRemote);
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);

    const semanticEventArgs = await testBrowserProxy.handler.getArgs(
        'recordLensOverlaySemanticEvent');
    const semanticEvent = semanticEventArgs[semanticEventArgs.length - 1];
    assertEquals(SemanticEvent.kTextGleamsViewStart, semanticEvent);

    // When the detect text in region event is received, the context menu should
    // be shown without any detected text.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, 0);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, 2);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.text, 'hello there\r\ntest');
  });

  test('CopyRegionWordsFromFullTextResponse', async () => {
    await addGenericWordsToPageNormalized(callbackRouterRemote);

    let expectedStartIndex = -1;
    let expectedEndIndex = -1;
    let expectedText: string = '';
    const copyDetectedText: TextCopyCallback =
        (startIndex: number, endIndex: number, text: string) => {
          expectedStartIndex = startIndex;
          expectedEndIndex = endIndex;
          expectedText = text;
        };

    textLayerElement.onCopyDetectedText(/*startIndex=*/ 0,
                                        /*endIndex=*/ 2, copyDetectedText);
    assertEquals(expectedStartIndex, -1);
    assertEquals(expectedEndIndex, -1);
    assertEquals(expectedText, '');

    callCopyTextTimeout();
    assertEquals(expectedStartIndex, 0);
    assertEquals(expectedEndIndex, 2);
    assertEquals(expectedText, 'hello there\r\ntest');
  });

  test('CopyRegionWordsFromRegionResponse', async () => {
    await addEmptyTextToPage(callbackRouterRemote);

    let expectedStartIndex = -1;
    let expectedEndIndex = -1;
    let expectedText: string = '';
    const copyDetectedText: TextCopyCallback =
        (startIndex: number, endIndex: number, text: string) => {
          expectedStartIndex = startIndex;
          expectedEndIndex = endIndex;
          expectedText = text;
        };

    textLayerElement.onCopyDetectedText(/*startIndex=*/ 0,
                                        /*endIndex=*/ 2, copyDetectedText);
    assertEquals(expectedStartIndex, -1);
    assertEquals(expectedEndIndex, -1);
    assertEquals(expectedText, '');
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);

    assertEquals(expectedStartIndex, 0);
    assertEquals(expectedEndIndex, 2);
    assertEquals(expectedText, 'hello there\r\ntest');
    assertTrue(textLayerElement.getHasActionedTextForTesting());
  });

  test('TranslateRegionWordsFromFullTextResponse', async () => {
    await addGenericWordsToPageNormalized(callbackRouterRemote);

    textLayerElement.selectAndTranslateWords(/*startIndex=*/ 0,
                                             /*endIndex=*/ 2);
    callTranslateTextTimeout();
    const textQuery = await testBrowserProxy.handler.whenCalled(
        'issueTranslateSelectionRequest');
    assertDeepEquals('hello there test', textQuery);
    assertTrue(textLayerElement.getHasActionedTextForTesting());
  });

  test('TranslateRegionWordsFromRegionTextResponse', async () => {
    // The first text received will be part of the full image response.
    await addEmptyTextToPage(callbackRouterRemote);

    textLayerElement.selectAndTranslateWords(/*startIndex=*/ 0,
                                             /*endIndex=*/ 2);
    // The next text received will be considered part of the region response.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    const textQuery = await testBrowserProxy.handler.whenCalled(
        'issueTranslateSelectionRequest');
    assertDeepEquals('hello there test', textQuery);
    assertTrue(textLayerElement.getHasActionedTextForTesting());
  });

  test('ShowHighlightedRegionText', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Add 3 words to the region text response.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);

    const highlightedLineElements: NodeListOf<Element> =
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line');
    assertEquals(2, highlightedLineElements.length);
    const bodyRect = document.body.getBoundingClientRect();
    const threshold = 1e-4;

    const firstHighlightedLine = highlightedLineElements.item(0);
    const rect = firstHighlightedLine.getBoundingClientRect();

    const expectedLine1 = {x: 0.105, y: 0.105, width: 0.11, height: 0.11};
    assertWithinThreshold(
        expectedLine1.width, rect.width / bodyRect.width, threshold);
    assertWithinThreshold(
        expectedLine1.height, rect.height / bodyRect.height, threshold);
    assertWithinThreshold(
        (expectedLine1.x - expectedLine1.width / 2), rect.left / bodyRect.width,
        threshold);
    assertWithinThreshold(
        (expectedLine1.y - expectedLine1.height / 2),
        rect.top / bodyRect.height, threshold);

    const secondLine = highlightedLineElements.item(1);
    const secondRect = secondLine.getBoundingClientRect();

    const expectedLine2 = {x: 0.3, y: 0.3, width: 0.1, height: 0.1};
    assertWithinThreshold(
        expectedLine2.width, secondRect.width / bodyRect.width, threshold);
    assertWithinThreshold(
        expectedLine2.height, secondRect.height / bodyRect.height, threshold);
    assertWithinThreshold(
        (expectedLine2.x - expectedLine2.width / 2),
        secondRect.left / bodyRect.width, threshold);
    assertWithinThreshold(
        (expectedLine2.y - expectedLine2.height / 2),
        secondRect.top / bodyRect.height, threshold);
    assertFalse(textLayerElement.getHasActionedTextForTesting());
  });

  test('NewRegionTextClearsHighlights', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Add 3 words to the region text response.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);
    assertEquals(
        2,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Getting a follow-up text response should clear highlights.
    await addEmptyRegionTextToPage(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);
    assertEquals(
        0,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Add 3 words to the region text response.
    await addRegionTextToPage(callbackRouterRemote, TOP_TO_BOTTOM_TEXT);
    await waitAfterNextRender(textLayerElement);
    assertEquals(
        2,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);
  });

  test('ShowHighlightedRegionTextCurvedText', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Add 3 words with writing direction kTopToBottom.
    await addRegionTextToPage(callbackRouterRemote, CURVED_TEXT);
    await waitAfterNextRender(textLayerElement);

    const highlightedLineElements: NodeListOf<HTMLElement> =
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line');
    assertEquals(2, highlightedLineElements.length);
    const bodyRect = document.body.getBoundingClientRect();
    const threshold = 1e-2;

    const firstHighlightedLine = highlightedLineElements.item(0);
    const expectedLine1 = {x: 0.105, y: 0.105, width: 0.11, height: 0.11};
    assertWithinThreshold(
        expectedLine1.width, firstHighlightedLine.offsetWidth / bodyRect.width,
        threshold);
    assertWithinThreshold(
        expectedLine1.height,
        firstHighlightedLine.offsetHeight / bodyRect.height, threshold);
    assertWithinThreshold(
        (expectedLine1.x - expectedLine1.width / 2),
        firstHighlightedLine.offsetLeft / bodyRect.width, threshold);
    assertWithinThreshold(
        (expectedLine1.y - expectedLine1.height / 2),
        firstHighlightedLine.offsetTop / bodyRect.height, threshold);

    const secondLine = highlightedLineElements.item(1);
    const expectedLine2 = {x: 0.3, y: 0.3, width: 0.1, height: 0.1};
    assertWithinThreshold(
        expectedLine2.width, secondLine.offsetWidth / bodyRect.width,
        threshold);
    assertWithinThreshold(
        expectedLine2.height, secondLine.offsetHeight / bodyRect.height,
        threshold);
    assertWithinThreshold(
        (expectedLine2.x - expectedLine2.width / 2),
        secondLine.offsetLeft / bodyRect.width, threshold);
    assertWithinThreshold(
        (expectedLine2.y - expectedLine2.height / 2),
        secondLine.offsetTop / bodyRect.height, threshold);
  });

  test('ShowHighlightedRegionTextTopToBottomWritingDirection', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Add 3 words with writing direction kTopToBottom.
    await addRegionTextToPage(callbackRouterRemote, TOP_TO_BOTTOM_TEXT);
    await waitAfterNextRender(textLayerElement);

    const highlightedLineElements: NodeListOf<Element> =
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line');
    assertEquals(2, highlightedLineElements.length);
    const bodyRect = document.body.getBoundingClientRect();
    const threshold = 1e-4;

    const firstHighlightedLine = highlightedLineElements.item(0);
    const rect = firstHighlightedLine.getBoundingClientRect();

    const expectedLine1 = {x: 0.11, y: 0.1025, width: 0.12, height: 0.115};
    assertWithinThreshold(
        expectedLine1.width, rect.width / bodyRect.width, threshold);
    assertWithinThreshold(
        expectedLine1.height, rect.height / bodyRect.height, threshold);
    assertWithinThreshold(
        (expectedLine1.x - expectedLine1.width / 2), rect.left / bodyRect.width,
        threshold);
    assertWithinThreshold(
        (expectedLine1.y - expectedLine1.height / 2),
        rect.top / bodyRect.height, threshold);

    const secondLine = highlightedLineElements.item(1);
    const secondRect = secondLine.getBoundingClientRect();

    const expectedLine2 = {x: 0.3, y: 0.3, width: 0.1, height: 0.11};
    assertWithinThreshold(
        expectedLine2.width, secondRect.width / bodyRect.width, threshold);
    assertWithinThreshold(
        expectedLine2.height, secondRect.height / bodyRect.height, threshold);
    assertWithinThreshold(
        (expectedLine2.x - expectedLine2.width / 2),
        secondRect.left / bodyRect.width, threshold);
    assertWithinThreshold(
        (expectedLine2.y - expectedLine2.height / 2),
        secondRect.top / bodyRect.height, threshold);
  });

  test('IgnoreTextReceivedWhileSelectingRegion', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // There should no highlighted lines.
    await waitAfterNextRender(textLayerElement);
    assertEquals(
        0,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Receiving text mid-selection should not be used.
    textLayerElement.onSelectionStart();
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    textLayerElement.onSelectionFinish();

    // There should still be no highlighted lines.
    await waitAfterNextRender(textLayerElement);
    assertEquals(
        0,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Text receievd now should render highlighted lines on the overlay.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);
    assertEquals(
        2,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);
  });

  test('UpdateContextMenuIfAlreadyShown', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Simulate a new selection being created.
    textLayerElement.onSelectionStart();
    textLayerElement.onSelectionFinish();

    // Add 3 words to the region text response.
    await addGenericWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);

    const showSelectedRegionContextMenuEvent =
        eventToPromise('show-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    await showSelectedRegionContextMenuEvent;

    const updateSelectedRegionContextMenuEventPromise =
        eventToPromise('update-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    await updateSelectedRegionContextMenuEventPromise;

    // Simulate another selection being created.
    textLayerElement.onSelectionStart();
    textLayerElement.onSelectionFinish();

    const showSelectedRegionContextMenuEvent2 =
        eventToPromise('show-selected-region-context-menu', document.body);
    await dispatchDetectTextInRegionEvent();
    await showSelectedRegionContextMenuEvent2;
  });

  test('ClearAllSelectionsClearsHighlightedLines', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Add 3 words to the region text response.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);

    assertFalse(textLayerElement.getHasActionedTextForTesting());
    assertEquals(
        2,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Simulate an action.
    textLayerElement.onCopyDetectedText(/*startIndex=*/ 0,
                                        /*endIndex=*/ 2,
                                        /*callback=*/ () => {});
    assertTrue(textLayerElement.getHasActionedTextForTesting());

    callbackRouterRemote.clearAllSelections();
    await flushTasks();
    await waitAfterNextRender(textLayerElement);

    assertFalse(textLayerElement.getHasActionedTextForTesting());
    assertEquals(
        0,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);
  });

  test('ClearRegionSelectionClearsHighlightedLines', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Add 3 words to the region text response.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);

    assertFalse(textLayerElement.getHasActionedTextForTesting());
    assertEquals(
        2,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Simulate an action.
    textLayerElement.onCopyDetectedText(/*startIndex=*/ 0,
                                        /*endIndex=*/ 2,
                                        /*callback=*/ () => {});
    assertTrue(textLayerElement.getHasActionedTextForTesting());

    callbackRouterRemote.clearRegionSelection();
    await flushTasks();
    await waitAfterNextRender(textLayerElement);

    assertFalse(textLayerElement.getHasActionedTextForTesting());
    assertEquals(
        0,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);
  });

  test('OnOverlayReshownClearsState', async () => {
    await addEmptyTextToPage(callbackRouterRemote);
    // Add 3 words to the region text response.
    await addGenericRegionWordsToPageNormalized(callbackRouterRemote);
    await waitAfterNextRender(textLayerElement);

    assertFalse(textLayerElement.getHasActionedTextForTesting());
    assertEquals(
        2,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Simulate an action.
    textLayerElement.onCopyDetectedText(/*startIndex=*/ 0,
                                        /*endIndex=*/ 2,
                                        /*callback=*/ () => {});
    assertTrue(textLayerElement.getHasActionedTextForTesting());

    callbackRouterRemote.onOverlayReshown(fakeScreenshotBitmap(100, 100));
    await flushTasks();
    await waitAfterNextRender(textLayerElement);

    assertFalse(textLayerElement.getHasActionedTextForTesting());
    assertEquals(
        0,
        textLayerElement.shadowRoot.querySelectorAll('.highlighted-line')
            .length);

    // Verify text responses are cleared by simulating a new selection and
    // asserting no text is detected.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);
    callTextReceivedTimeout();
    await dispatchDetectTextInRegionEvent();
    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, -1);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, -1);
  });
});
