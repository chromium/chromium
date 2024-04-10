// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/selection_overlay.js';

import type {Point, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens/selection_overlay.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('ManualRegionSelection', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;

  setup(() => {
    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    selectionOverlayElement = document.createElement('lens-selection-overlay');
    // Position absolutely so we can handle logic of drag ending off this
    // element.
    selectionOverlayElement.style.position = 'absolute';
    selectionOverlayElement.style.height = 'calc(100% - 100px)';
    selectionOverlayElement.style.width = 'calc(100% - 100px)';
    selectionOverlayElement.style.top = '50px';
    selectionOverlayElement.style.left = '50px';
    document.body.appendChild(selectionOverlayElement);
    return waitAfterNextRender(selectionOverlayElement);
  });

  function createPrimaryClickPointerEvent(
      eventType: string, point: Point): PointerEvent {
    return new PointerEvent(eventType, {
      pointerId: 1,
      bubbles: true,
      button: 0,
      clientX: point.x,
      clientY: point.y,
      isPrimary: true,
    });
  }

  function doDrag(fromPoint: Point, toPoint: Point): Promise<void> {
    const pointerDownEvent =
        createPrimaryClickPointerEvent('pointerdown', fromPoint);
    const pointerMoveEvent =
        createPrimaryClickPointerEvent('pointermove', toPoint);
    const pointerUpEvent = createPrimaryClickPointerEvent('pointerup', toPoint);

    selectionOverlayElement.dispatchEvent(pointerDownEvent);
    selectionOverlayElement.dispatchEvent(pointerMoveEvent);
    selectionOverlayElement.dispatchEvent(pointerUpEvent);
    return flushTasks();
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

  // Does a drag and verifies that expectedRect is sent via mojo.
  async function assertDragGestureSendsRequest(
      fromPoint: Point, toPoint: Point, expectedRect: CenterRotatedBox) {
    // Ensures the whenCalled method returns because of our drag, not a leftover
    // call that already happened.
    testBrowserProxy.handler.resetResolver('issueLensRequest');

    await doDrag(fromPoint, toPoint);
    const rect = await testBrowserProxy.handler.whenCalled('issueLensRequest');
    assertDeepEquals(expectedRect, rect);
  }

  test(
      `verify that completing a drag within the overlay bounds issues correct
      lens request via mojo`,
      async () => {
        const overlayRect = selectionOverlayElement.getBoundingClientRect();
        const startPointInsideOverlay = {
          x: overlayRect.left + 10,
          y: overlayRect.top + 10,
        };
        const endPointInsideOverlay = {
          x: overlayRect.left + 100,
          y: overlayRect.top + 100,
        };

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox({x: 55, y: 55, width: 90, height: 90}),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        await assertDragGestureSendsRequest(
            startPointInsideOverlay, endPointInsideOverlay, expectedRect);
      });

  test(
      'verify that completing a drag above the selection overlay rounds y to 0',
      async () => {
        const overlayRect = selectionOverlayElement.getBoundingClientRect();
        const startPointInsideOverlay = {
          x: overlayRect.left + 10,
          y: overlayRect.top + 10,
        };
        const endPointAboveOverlay = {
          x: overlayRect.left + 100,
          y: overlayRect.top - 30,
        };

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox({x: 55, y: 5, width: 90, height: 10}),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        await assertDragGestureSendsRequest(
            startPointInsideOverlay, endPointAboveOverlay, expectedRect);
      });

  test(
      `verify that completing a drag below the selection overlay rounds y to
      overlay height`,
      async () => {
        const overlayRect = selectionOverlayElement.getBoundingClientRect();
        const startPointInsideOverlay = {
          x: overlayRect.left + 10,
          y: overlayRect.bottom - 20,
        };
        const endPointBelowOverlay = {
          x: overlayRect.left + 100,
          y: overlayRect.bottom + 20,
        };

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox(
              {x: 55, y: overlayRect.height - 10, width: 90, height: 20}),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        await assertDragGestureSendsRequest(
            startPointInsideOverlay, endPointBelowOverlay, expectedRect);
      });

  test(
      `verify that completing a drag to the left of the selection overlay rounds
       x to 0`,
      async () => {
        const overlayRect = selectionOverlayElement.getBoundingClientRect();
        const startPointInsideOverlay = {
          x: overlayRect.left + 20,
          y: overlayRect.top + 10,
        };
        const endPointLeftOfOverlay = {
          x: overlayRect.left - 10,
          y: overlayRect.top + 100,
        };

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox({x: 10, y: 55, width: 20, height: 90}),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        await assertDragGestureSendsRequest(
            startPointInsideOverlay, endPointLeftOfOverlay, expectedRect);
      });

  test(
      `verify that completing a drag to the right of the selection overlay
      rounds x to overlay width`,
      async () => {
        const overlayRect = selectionOverlayElement.getBoundingClientRect();
        const startPointInsideOverlay = {
          x: overlayRect.right - 20,
          y: overlayRect.top + 10,
        };
        const endPointRightOfOverlay = {
          x: overlayRect.right + 10,
          y: overlayRect.top + 100,
        };

        const expectedRect: CenterRotatedBox = {
          box: normalizedBox(
              {x: overlayRect.width - 10, y: 55, width: 20, height: 90}),
          rotation: 0,
          coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
        };
        await assertDragGestureSendsRequest(
            startPointInsideOverlay, endPointRightOfOverlay, expectedRect);
      });

  test('verify canvas resizes', async () => {
    selectionOverlayElement.$.regionSelectionLayer.setCanvasSizeTo(50, 50);
    await waitAfterNextRender(selectionOverlayElement.$.regionSelectionLayer);
    assertEquals(
        50,
        selectionOverlayElement.$.regionSelectionLayer.$.regionSelectionCanvas
            .width);
    assertEquals(
        50,
        selectionOverlayElement.$.regionSelectionLayer.$.regionSelectionCanvas
            .height);

    selectionOverlayElement.$.regionSelectionLayer.setCanvasSizeTo(100, 100);
    await waitAfterNextRender(selectionOverlayElement.$.regionSelectionLayer);
    assertEquals(
        100,
        selectionOverlayElement.$.regionSelectionLayer.$.regionSelectionCanvas
            .width);
    assertEquals(
        100,
        selectionOverlayElement.$.regionSelectionLayer.$.regionSelectionCanvas
            .height);
  });
});
