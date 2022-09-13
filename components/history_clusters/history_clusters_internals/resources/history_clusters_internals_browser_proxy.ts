// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './history_clusters_internals.mojom-webui.js';

export class HistoryClustersInternalsBrowserProxy {
  private callbackRouter: PageCallbackRouter;
  private handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    const factory = PageHandlerFactory.getRemote();
    this.handler = new PageHandlerRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): HistoryClustersInternalsBrowserProxy {
    return instance || (instance = new HistoryClustersInternalsBrowserProxy());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }

  getHandler(): PageHandlerRemote {
    return this.handler;
  }
}

let instance: HistoryClustersInternalsBrowserProxy|null = null;