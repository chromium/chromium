// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome-untrusted://lens/browser_proxy.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import {LensPageCallbackRouter, type LensPageHandlerInterface} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

/**
 * Test version of the LensPageHandler used to verify calls to the browser from
 * WebUI.
 */
export class TestLensOverlayPageHandler extends TestBrowserProxy implements
    LensPageHandlerInterface {
  constructor() {
    super(['closeRequestedByOverlay', 'issueLensRequest']);
  }

  closeRequestedByOverlay() {
    this.methodCalled('closeRequestedByOverlay');
  }

  issueLensRequest(rect: CenterRotatedBox) {
    this.methodCalled('issueLensRequest', rect);
  }
}

/**
 * Test version of the BrowserProxy used in connecting Lens Overlay to the
 * browser on start up.
 */
export class TestLensOverlayBrowserProxy implements BrowserProxy {
  callbackRouter: LensPageCallbackRouter = new LensPageCallbackRouter();
  handler: TestLensOverlayPageHandler = new TestLensOverlayPageHandler();
}
