// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './data_sharing_internals.mojom-webui.js';

export class DataSharingInternalsBrowserProxy {
  private handler: PageHandlerRemote;
  private callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getAllGroups() {
    return this.handler.getAllGroups();
  }

  static getInstance(): DataSharingInternalsBrowserProxy {
    return instance || (instance = new DataSharingInternalsBrowserProxy());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }
}

let instance: DataSharingInternalsBrowserProxy|null = null;
