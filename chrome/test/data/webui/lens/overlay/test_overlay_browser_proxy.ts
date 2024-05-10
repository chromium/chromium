// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome-untrusted://lens/browser_proxy.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {LensPageHandlerInterface, LensPageRemote} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import {LensPageCallbackRouter} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

/**
 * Test version of the LensPageHandler used to verify calls to the browser from
 * WebUI.
 */
export class TestLensOverlayPageHandler extends TestBrowserProxy implements
    LensPageHandlerInterface {
  constructor() {
    super([
      'closeRequestedByOverlayCloseButton',
      'closeRequestedByOverlayBackgroundClick',
      'addBackgroundBlur',
      'closeSearchBubble',
      'feedbackRequestedByOverlay',
      'issueLensRequest',
      'issueTextSelectionRequest',
    ]);
  }

  closeRequestedByOverlayCloseButton() {
    this.methodCalled('closeRequestedByOverlayCloseButton');
  }

  closeRequestedByOverlayBackgroundClick() {
    this.methodCalled('closeRequestedByOverlayBackgroundClick');
  }

  addBackgroundBlur() {
    this.methodCalled('addBackgroundBlur');
  }

  closeSearchBubble() {
    this.methodCalled('closeSearchBubble');
  }

  feedbackRequestedByOverlay() {
    this.methodCalled('feedbackRequestedByOverlay');
  }

  issueLensRequest(rect: CenterRotatedBox) {
    this.methodCalled('issueLensRequest', rect);
  }

  issueTextSelectionRequest(query: string) {
    this.methodCalled('issueTextSelectionRequest', query);
  }
}

/**
 * Test version of the BrowserProxy used in connecting Lens Overlay to the
 * browser on start up.
 */
export class TestLensOverlayBrowserProxy implements BrowserProxy {
  callbackRouter: LensPageCallbackRouter = new LensPageCallbackRouter();
  handler: TestLensOverlayPageHandler = new TestLensOverlayPageHandler();
  page: LensPageRemote = this.callbackRouter.$.bindNewPipeAndPassRemote();
}
