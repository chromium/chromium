// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/lens_overlay_app.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens-overlay/lens_overlay_app.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';
import {assertTrue, assertFalse} from 'chrome-untrusted://webui-test/chai_assert.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('Searchbox', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      'enableOverlayContextualSearchbox': true,
    });

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    await waitAfterNextRender(lensOverlayElement);

    testBrowserProxy.page.shouldShowContextualSearchBox(true);
    await waitAfterNextRender(lensOverlayElement);
  });

  test('SearchBoxHidesWhenSidePanelOpens', async () => {
    assertTrue(isVisible(lensOverlayElement.$.searchbox));

    testBrowserProxy.page.notifyResultsPanelOpened();
    await waitAfterNextRender(lensOverlayElement);

    assertFalse(isVisible(lensOverlayElement.$.searchbox));
  });
});
