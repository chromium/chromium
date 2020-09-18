// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class BrowserProxy {
  constructor() {
    /** @type {chromeos.fileManager.mojom.PageCallbackRouter} */
    this.callbackRouter = new chromeos.fileManager.mojom.PageCallbackRouter();
    /** @type {chromeos.fileManager.mojom.PageHandlerRemote} */
    this.handler = new chromeos.fileManager.mojom.PageHandlerRemote();

    const factory = chromeos.fileManager.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}

// For demo purpose a global variable is fine I guess.
// I'll defer to people who know how to JS to do it in a proper way.
const theBrowserProxy = new BrowserProxy();
