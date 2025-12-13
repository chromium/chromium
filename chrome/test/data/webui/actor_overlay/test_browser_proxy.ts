// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActorOverlayPageHandlerInterface} from 'chrome://actor-overlay/actor_overlay.mojom-webui.js';
import {ActorOverlayPageCallbackRouter} from 'chrome://actor-overlay/actor_overlay.mojom-webui.js';
import type {ActorOverlayPageRemote} from 'chrome://actor-overlay/actor_overlay.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestActorOverlayBrowserProxy {
  callbackRouter: ActorOverlayPageCallbackRouter =
      new ActorOverlayPageCallbackRouter();
  handler: TestActorOverlayPageHandler;
  remote: ActorOverlayPageRemote;

  constructor() {
    this.handler = new TestActorOverlayPageHandler();
    this.remote = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

export class TestActorOverlayPageHandler extends TestBrowserProxy implements
    ActorOverlayPageHandlerInterface {
  private isBorderGlowVisible_: boolean = false;

  constructor() {
    super(['onHoverStatusChanged', 'getCurrentBorderGlowVisibility']);
  }

  onHoverStatusChanged(isVisible: boolean) {
    this.methodCalled('onHoverStatusChanged', isVisible);
  }

  getCurrentBorderGlowVisibility() {
    this.methodCalled('getCurrentBorderGlowVisibility');
    return Promise.resolve({isVisible: this.isBorderGlowVisible_});
  }

  setBorderGlowVisibilityForTesting(isVisible: boolean) {
    this.isBorderGlowVisible_ = isVisible;
  }
}
