// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/lens_overlay_app.js';

import {BrowserProxyImpl} from 'chrome-untrusted://lens/browser_proxy.js';
import type {LensOverlayAppElement} from 'chrome-untrusted://lens/lens_overlay_app.js';
import {waitBeforeNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';

suite('OverlayCloseButton', () => {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let lensOverlayElement: LensOverlayAppElement;

  setup(() => {
    testBrowserProxy = new TestLensOverlayBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensOverlayElement = document.createElement('lens-overlay-app');
    document.body.appendChild(lensOverlayElement);
    return waitBeforeNextRender(lensOverlayElement);
  });

  test('verify clicking close button calls browser proxy', () => {
    lensOverlayElement.$.closeButton.click();
    return testBrowserProxy.handler.whenCalled('closeRequestedByOverlay');
  });
});
