// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  PageCallbackRouter,
  PageConnector,
} from './kiosk_vision_internals.mojom-webui.js';

export class BrowserProxy {
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    const connector = PageConnector.getRemote();
    connector.bindPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy | null = null;

