// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolicyPageClientCallbackRouter, PolicyPageHandlerFactory, PolicyPageHandlerRemote} from './policy.mojom-webui.js';

export class BrowserProxy {
  handler: PolicyPageHandlerRemote;
  callbackRouter: PolicyPageClientCallbackRouter;

  constructor() {
    this.handler = new PolicyPageHandlerRemote();
    this.callbackRouter = new PolicyPageClientCallbackRouter();

    PolicyPageHandlerFactory.getRemote().createHandler(
        this.handler.$.bindNewPipeAndPassReceiver(),
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return proxyInstance || (proxyInstance = new BrowserProxy());
  }
}
let proxyInstance: BrowserProxy|null = null;
