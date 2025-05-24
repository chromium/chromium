// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/lens_overlay_app.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens-overlay/lens_overlay_app.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

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

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    return waitAfterNextRender(lensOverlayElement);
  });

  // Verify average FPS gets logged after overlay notify closing.
  test('LogsAverageFps', async () => {
    // Wait for at least one frame.
    await waitAfterNextRender(lensOverlayElement);

    // Notify overlay is closing.
    testBrowserProxy.page.notifyOverlayClosing();
    await flushTasks();

    // Verify average FPS gets logged.
    assertEquals(1, metrics.count('Lens.Overlay.Performance.AverageFPS'));
  });
});
