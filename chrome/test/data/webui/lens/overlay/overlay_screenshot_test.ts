// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/lens_overlay_app.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens-overlay/lens_overlay_app.js';
import {ScreenshotBitmapBrowserProxyImpl} from 'chrome-untrusted://lens-overlay/screenshot_bitmap_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {hasStyle} from 'chrome-untrusted://webui-test/test_util.js';

import {fakeScreenshotBitmap, waitForScreenshotRendered} from '../utils/image_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('OverlayScreenshot', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;
  let metrics: MetricsTracker;

  setup(() => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = fakeMetricsPrivate();

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // Reset the screenshot bitmap browser proxy to a new instance for each
    // test.
    ScreenshotBitmapBrowserProxyImpl.setInstance(
        new ScreenshotBitmapBrowserProxyImpl());

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    waitAfterNextRender(lensOverlayElement);
    return assertEquals(1, metrics.count('Lens.Overlay.TimeToWebUIReady'));
  });

  // Verify selection overlay is hidden until screenshot data URI is received
  test('ShowSelectionOverlay', async () => {
    const appContainerBeforeScreenshot =
        lensOverlayElement.shadowRoot!.querySelector('.app-container');
    assertTrue(!!appContainerBeforeScreenshot);
    const selectionOverlayBeforeScreenshot =
        appContainerBeforeScreenshot.querySelector('lens-selection-overlay');
    assertTrue(!!selectionOverlayBeforeScreenshot);
    assertTrue(hasStyle(selectionOverlayBeforeScreenshot, 'display', 'none'));

    // The following struct needs to be casted as BigBuffer in order to set
    // undefined values without breaking assertions by setting them
    // directly.
    testBrowserProxy.page.screenshotDataReceived(
        fakeScreenshotBitmap(), /*isSidePanelOpen=*/ false);
    await waitForScreenshotRendered(selectionOverlayBeforeScreenshot);

    const appContainer =
        lensOverlayElement.shadowRoot!.querySelector('.app-container');
    assertTrue(!!appContainer);
    const selectionOverlay =
        appContainer.querySelector('lens-selection-overlay');
    assertTrue(!!selectionOverlay);
    assertFalse(hasStyle(selectionOverlayBeforeScreenshot, 'display', 'none'));
  });

  test('ScreenshotWithSidePanel', async () => {
    const appContainerBeforeScreenshot =
        lensOverlayElement.shadowRoot!.querySelector('.app-container');
    assertTrue(!!appContainerBeforeScreenshot);
    const selectionOverlayBeforeScreenshot =
        appContainerBeforeScreenshot.querySelector('lens-selection-overlay');
    assertTrue(!!selectionOverlayBeforeScreenshot);
    assertTrue(hasStyle(selectionOverlayBeforeScreenshot, 'display', 'none'));

    testBrowserProxy.page.screenshotDataReceived(
        fakeScreenshotBitmap(), /*isSidePanelOpen=*/ true);
    await waitForScreenshotRendered(selectionOverlayBeforeScreenshot);
    await waitAfterNextRender(lensOverlayElement);

    const appContainer =
        lensOverlayElement.shadowRoot!.querySelector('.app-container');
    assertTrue(!!appContainer);
    const selectionOverlay =
        appContainer.querySelector('lens-selection-overlay');
    assertTrue(!!selectionOverlay);
    assertTrue(lensOverlayElement.getSidePanelOpenedForTesting());
    assertFalse(hasStyle(selectionOverlayBeforeScreenshot, 'display', 'none'));
  });

  test('ScreenshotWithSidePanelAndReshow', async () => {
    const selectionOverlay =
        lensOverlayElement.shadowRoot!.querySelector('lens-selection-overlay');
    assertTrue(!!selectionOverlay);

    assertFalse(lensOverlayElement.getSidePanelOpenedForTesting());
    testBrowserProxy.page.screenshotDataReceived(
        fakeScreenshotBitmap(), /*isSidePanelOpen=*/ false);
    await waitForScreenshotRendered(selectionOverlay);

    assertFalse(lensOverlayElement.getSidePanelOpenedForTesting());
    assertFalse(lensOverlayElement.getOverlayReshowInProgressForTesting());
    testBrowserProxy.page.onOverlayReshown(fakeScreenshotBitmap());

    await testBrowserProxy.handler.whenCalled('finishReshowOverlay');
    assertTrue(lensOverlayElement.getSidePanelOpenedForTesting());
    assertFalse(lensOverlayElement.getOverlayReshowInProgressForTesting());
    assertEquals(
        1, testBrowserProxy.handler.getCallCount('finishReshowOverlay'));
  });
});
