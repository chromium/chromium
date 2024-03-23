// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/selection_overlay.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens/selection_overlay.js';
import {assertDeepEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
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
    document.body.appendChild(selectionOverlayElement);
    return waitAfterNextRender(selectionOverlayElement);
  });

  function createPrimaryClickPointerEvent(
      eventType: string, point: {x: number, y: number}): PointerEvent {
    return new PointerEvent(eventType, {
      pointerId: 1,
      bubbles: true,
      button: 0,
      clientX: point.x,
      clientY: point.y,
      isPrimary: true,
    });
  }

  function doDrag(
      fromPoint: {x: number, y: number},
      toPoint: {x: number, y: number}): Promise<void> {
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

  test(
      'verify that completing a drag issues lens request via mojo',
      async () => {
        await doDrag({x: 10, y: 10}, {x: 100, y: 100});
        const expectedRect: RectF = {x: 10, y: 10, width: 90, height: 90};
        const rect =
            await testBrowserProxy.handler.whenCalled('issueLensRequest');
        assertDeepEquals(expectedRect, rect);
      });
});
