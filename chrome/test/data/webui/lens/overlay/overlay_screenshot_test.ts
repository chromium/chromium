// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/lens_overlay_app.js';

import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens/lens_overlay_app.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {hasStyle} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

// Default screenshot data URI is a 1600x1 pink rectangle.
const SCREENSHOT_DATA_URI =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAABkAAAAABCAYAAACG2GJUAAAA' +
    'AXNSR0IArs4c6QAAADVJREFUWEft0AENAAAMAiDfP7T2+CAC17TBgAEDBgwYMGDAgAEDBg' +
    'wYMGDAgAEDBgwYMPBoYOZdAv///pRmAAAAAElFTkSuQmCC';

suite('OverlayScreenshot', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;

  setup(() => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    return waitAfterNextRender(lensOverlayElement);
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

    const dataUriBytes = new TextEncoder().encode(SCREENSHOT_DATA_URI);
    // The following struct needs to be casted as BigBuffer in order to set
    // undefined values without breaking assertions by setting them
    // directly.
    testBrowserProxy.page.screenshotDataUriReceived({
      data: {
        bytes: Array.from(dataUriBytes),
      } as BigBuffer,
    });
    await waitAfterNextRender(lensOverlayElement);

    const appContainer =
        lensOverlayElement.shadowRoot!.querySelector('.app-container');
    assertTrue(!!appContainer);
    const selectionOverlay =
        appContainer.querySelector('lens-selection-overlay');
    assertTrue(!!selectionOverlay);
    assertFalse(hasStyle(selectionOverlayBeforeScreenshot, 'display', 'none'));
  });
});
