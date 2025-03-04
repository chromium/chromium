// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://lens-overlay/selection_overlay.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {SemanticEvent} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {SimplifiedTextLayerElement} from 'chrome-untrusted://lens-overlay/simplified_text_layer.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome-untrusted://webui-test/test_util.js';

import {normalizeBoxInElement} from '../utils/selection_utils.js';
import {addEmptyTextToPage, addGenericWordsToPage} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

const TEXT_RECEIVED_TIMEOUT_MS = 1000000;

suite('SimplifiedSelection', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let textLayerElement: SimplifiedTextLayerElement;
  let callbackRouterRemote: LensPageRemote;
  let textReceivedTimeoutFunction: Function|undefined;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      'textReceivedTimeout': TEXT_RECEIVED_TIMEOUT_MS,
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
      return origSetTimeout(handler, timeout);
    };

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    textLayerElement = document.createElement('lens-simplified-text-layer');
    document.body.appendChild(textLayerElement);
    await waitAfterNextRender(textLayerElement);

    textLayerElement.onSelectionStart();
    textLayerElement.onSelectionFinish();
  });

  teardown(() => {
    textReceivedTimeoutFunction = undefined;
  });

  function callTextReceivedTimeout() {
    assertTrue(textReceivedTimeoutFunction !== undefined);
    textReceivedTimeoutFunction();
  }

  async function dispatchDetextTextInRegionEvent() {
    const centerRotatedBox = {
      box: normalizeBoxInElement(
          {x: 10, y: 10, width: 10, height: 10}, textLayerElement),
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    };
    textLayerElement.dispatchEvent(new CustomEvent('detect-text-in-region', {
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
    await dispatchDetextTextInRegionEvent();
    await hideSelectedRegionContextMenuEventPromise;
  });

  test('OnSelectionFinishedClearsText', async () => {
    const receivedTextEventPromise =
        eventToPromise('finished-receiving-text', document.body);
    await addGenericWordsToPage(callbackRouterRemote, textLayerElement);
    await receivedTextEventPromise;

    // Simulate a new selection being created.
    textLayerElement.onSelectionStart();
    textLayerElement.onSelectionFinish();

    // When the detect text in region event is received, the context menu should
    // be shown without any detected text.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);

    // Call timeout to simulate no text being received.
    callTextReceivedTimeout();
    await dispatchDetextTextInRegionEvent();

    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, -1);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, -1);
  });

  test('HideContextMenuTimeoutOngoingNoText', async () => {
    const hideSelectedRegionContextMenuEventPromise =
        eventToPromise('hide-selected-region-context-menu', document.body);
    await dispatchDetextTextInRegionEvent();
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
        await dispatchDetextTextInRegionEvent();
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

    // When the detect text in region event is received, the context menu should
    // be shown without any detected text.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);
    await dispatchDetextTextInRegionEvent();
    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, -1);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, -1);
  });

  test('SelectedRegionContextMenuAppearsWithText', async () => {
    await addGenericWordsToPage(callbackRouterRemote, textLayerElement);

    const semanticEventArgs = await testBrowserProxy.handler.getArgs(
        'recordLensOverlaySemanticEvent');
    const semanticEvent = semanticEventArgs[semanticEventArgs.length - 1];
    assertEquals(SemanticEvent.kTextGleamsViewStart, semanticEvent);

    // When the detect text in region event is received, the context menu should
    // be shown without any detected text.
    const showSelectedRegionContextMenuEventPromise =
        eventToPromise('show-selected-region-context-menu', document.body);
    await dispatchDetextTextInRegionEvent();
    const showSelectedRegionContextMenuEvent =
        await showSelectedRegionContextMenuEventPromise;
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionStartIndex, 0);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.selectionEndIndex, 2);
    assertEquals(
        showSelectedRegionContextMenuEvent.detail.text, 'hello there\r\ntest');
  });

  test('TranslateRegionWords', async () => {
    await addGenericWordsToPage(callbackRouterRemote, textLayerElement);

    textLayerElement.selectAndTranslateWords(/*startIndex=*/ 0,
                                             /*endIndex=*/ 2);
    const textQuery = await testBrowserProxy.handler.whenCalled(
        'issueTranslateSelectionRequest');
    assertDeepEquals('hello there test', textQuery);
  });
});
