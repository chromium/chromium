// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/selection_overlay.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {ScreenshotBitmapBrowserProxyImpl} from 'chrome-untrusted://lens-overlay/screenshot_bitmap_browser_proxy.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens-overlay/selection_overlay.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome-untrusted://webui-test/test_util.js';

import {fakeScreenshotBitmap, waitForScreenshotRendered} from '../utils/image_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('ReshowOverlay', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;
  let callbackRouterRemote: LensPageRemote;

  async function waitForScreenshotResize(): Promise<void> {
    // The first render triggers the ResizeObserver. The second runs the
    // requestAnimationFrame callback queued by the ResizeObserver.
    await waitAfterNextRender(selectionOverlayElement);
    await waitAfterNextRender(selectionOverlayElement);
  }

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    loadTimeData.overrideValues({
      'enableShimmer': false,
    });

    // Recreate overlay element with new screenshot bitmap browser proxy.
    ScreenshotBitmapBrowserProxyImpl.setInstance(
        new ScreenshotBitmapBrowserProxyImpl());

    selectionOverlayElement = document.createElement('lens-selection-overlay');
    document.body.appendChild(selectionOverlayElement);
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';
    await waitAfterNextRender(selectionOverlayElement);
    return waitAfterNextRender(selectionOverlayElement);
  });

  test('verify screenshotDataReceived with side panel open', async () => {
    // ScreenshotBitmapBrowserProxy assumes only one screenshot will be sent.
    // We need to reset it to allow a new screenshot to be fetched.
    ScreenshotBitmapBrowserProxyImpl.setInstance(
        new ScreenshotBitmapBrowserProxyImpl());
    selectionOverlayElement.fetchNewScreenshotForTesting();

    // Send a fake screenshot of size 100x100 with side panel open.
    testBrowserProxy.page.screenshotDataReceived(
        fakeScreenshotBitmap(100, 100), /*isSidePanelOpen=*/ true);
    await waitForScreenshotRendered(selectionOverlayElement);
    await waitForScreenshotResize();

    assertTrue(selectionOverlayElement.hasAttribute('side-panel-opened'));
    assertTrue(selectionOverlayElement.hasAttribute('is-resized'));
    assertFalse(selectionOverlayElement.hasAttribute('is-initial-size'));
  });

  test(
      'verify screenshotDataReceived with side panel open disables context menu and text highlights',
      async () => {
        assertTrue(
            selectionOverlayElement.hasAttribute('enable-region-context-menu'));
        assertTrue(selectionOverlayElement.$.textLayer.hasAttribute(
            'enable-highlights'));

        // ScreenshotBitmapBrowserProxy assumes only one screenshot will be
        // sent. We need to reset it to allow a new screenshot to be fetched.
        ScreenshotBitmapBrowserProxyImpl.setInstance(
            new ScreenshotBitmapBrowserProxyImpl());
        selectionOverlayElement.fetchNewScreenshotForTesting();

        // Send a fake screenshot of size 100x100 with side panel open.
        testBrowserProxy.page.screenshotDataReceived(
            fakeScreenshotBitmap(100, 100), /*isSidePanelOpen=*/ true);
        await waitForScreenshotRendered(selectionOverlayElement);
        await waitForScreenshotResize();

        assertFalse(
            selectionOverlayElement.hasAttribute('enable-region-context-menu'));
        assertFalse(selectionOverlayElement.$.textLayer.hasAttribute(
            'enable-highlights'));
      });

  test(
      'verify onOverlayReshown hides and reshows the background image canvas',
      async () => {
        assertFalse(selectionOverlayElement.hasAttribute('is-closing'));
        assertFalse(selectionOverlayElement.hasAttribute('side-panel-opened'));
        assertTrue(
            selectionOverlayElement.hasAttribute('enable-region-context-menu'));
        assertTrue(selectionOverlayElement.$.textLayer.hasAttribute(
            'enable-highlights'));

        const finishReshowEvent =
            eventToPromise('on-finish-reshow-overlay', selectionOverlayElement);
        callbackRouterRemote.onOverlayReshown(fakeScreenshotBitmap(100, 100));
        await flushTasks();

        assertFalse(selectionOverlayElement.hasAttribute('is-closing'));
        assertTrue(selectionOverlayElement.hasAttribute('side-panel-opened'));
        assertTrue(
            selectionOverlayElement.getHideBackgroundImageCanvasForTesting());
        assertFalse(
            selectionOverlayElement.hasAttribute('enable-region-context-menu'));
        assertFalse(selectionOverlayElement.$.textLayer.hasAttribute(
            'enable-highlights'));
        await finishReshowEvent;

        // after onFinishReshowOverlay, hideBackgroundImageCanvas should be
        // false.
        assertFalse(selectionOverlayElement.hasAttribute('is-closing'));
        assertTrue(selectionOverlayElement.hasAttribute('side-panel-opened'));
        assertFalse(
            selectionOverlayElement.getHideBackgroundImageCanvasForTesting());
      });
});
