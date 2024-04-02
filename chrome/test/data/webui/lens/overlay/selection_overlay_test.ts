// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/selection_overlay.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens/selection_overlay.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {createLine, createParagraph, createText, createWord} from '../utils/text_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

interface Point {
  x: number;
  y: number;
}

suite('SelectionOverlay', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;
  let callbackRouterRemote: LensPageRemote;

  setup(() => {
    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    selectionOverlayElement = document.createElement('lens-selection-overlay');
    document.body.appendChild(selectionOverlayElement);
    // Since the size of the Selection Overlay is based on the screenshot which
    // is not loaded in the test, we need to force the overlay to take up the
    // viewport.
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';
    return flushTasks();
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
          createWord(
              'hello', normalizedBox({x: 20, y: 20, width: 30, height: 10})),
          createWord(
              'there', normalizedBox({x: 50, y: 20, width: 50, height: 10})),
          createWord(
              'test', normalizedBox({x: 80, y: 20, width: 30, height: 10})),
        ]),
      ]),
    ]);
    callbackRouterRemote.textReceived(text);
    return flushTasks();
  }

  function createPointerEvent(eventType: string, point: Point): PointerEvent {
    return new PointerEvent(eventType, {
      pointerId: 1,
      bubbles: true,
      button: 0,
      clientX: point.x,
      clientY: point.y,
      isPrimary: true,
    });
  }

  function simulateDrag(fromPoint: Point, toPoint: Point): Promise<void> {
    const pointerDownEvent = createPointerEvent('pointerdown', fromPoint);
    const pointerMoveEvent = createPointerEvent('pointermove', toPoint);
    const pointerUpEvent = createPointerEvent('pointerup', toPoint);

    selectionOverlayElement.dispatchEvent(pointerDownEvent);
    selectionOverlayElement.dispatchEvent(pointerMoveEvent);
    selectionOverlayElement.dispatchEvent(pointerUpEvent);
    return flushTasks();
  }

  test(
      'verify that starting a drag on a word does not trigger region search',
      async () => {
        await addWords();

        // Drag that starts on a word but finishes on empty space.
        const wordEl = selectionOverlayElement.$.textSelectionLayer
                           .getWordNodesForTesting()[0]!;
        await simulateDrag(
            {
              x: wordEl.getBoundingClientRect().left + 5,
              y: wordEl.getBoundingClientRect().top + 5,
            },
            {x: 0, y: 0});

        assertEquals(
            0, testBrowserProxy.handler.getCallCount('issueLensRequest'));
      });

  test(
      `verify that starting a drag off a word and continuing onto a word
      triggers region search`,
      async () => {
        await addWords();

        // Drag that starts off a word but finishes on a word.
        const wordEl = selectionOverlayElement.$.textSelectionLayer
                           .getWordNodesForTesting()[0]!;
        const dragEnd = {
          x: wordEl.getBoundingClientRect().left + 5,
          y: wordEl.getBoundingClientRect().top + 5,
        };
        await simulateDrag({x: 0, y: 0}, dragEnd);

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox({
            x: dragEnd.x / 2,
            y: dragEnd.y / 2,
            width: dragEnd.x,
            height: dragEnd.y,
          }),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        const rect =
            await testBrowserProxy.handler.whenCalled('issueLensRequest');
        assertDeepEquals(expectedRect, rect);
      });
});
