// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActorOverlayPageHandlerInterface} from 'chrome://actor-overlay/actor_overlay.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestActorOverlayBrowserProxy {
  handler: TestActorOverlayPageHandler;

  constructor() {
    this.handler = new TestActorOverlayPageHandler();
  }
}

export class TestActorOverlayPageHandler extends TestBrowserProxy implements
    ActorOverlayPageHandlerInterface {
  constructor() {
    super([
      'onHoverStatusChanged',
    ]);
  }

  onHoverStatusChanged(isVisible: boolean) {
    this.methodCalled('onHoverStatusChanged', isVisible);
  }
}
