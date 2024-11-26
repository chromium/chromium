// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './chrome_urls.mojom-webui.js';
import type {PageHandlerInterface} from './chrome_urls.mojom-webui.js';

export interface BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
}

let instance: BrowserProxy|null = null;

export class BrowserProxyImpl implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  private constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }
}
