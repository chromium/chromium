// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommerceInternalsHandlerFactory, CommerceInternalsHandlerRemote, CommerceInternalsPageCallbackRouter} from './commerce_internals.mojom-webui.js';

export class CommerceInternalsApiProxy {
  private callbackRouter: CommerceInternalsPageCallbackRouter;
  private handler: CommerceInternalsHandlerRemote;

  constructor() {
    this.callbackRouter = new CommerceInternalsPageCallbackRouter();
    this.handler = new CommerceInternalsHandlerRemote();
    const factory = CommerceInternalsHandlerFactory.getRemote();

    factory.createCommerceInternalsHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): CommerceInternalsApiProxy {
    return instance || (instance = new CommerceInternalsApiProxy());
  }

  getIsShoppingListEligible(): Promise<{eligible: boolean}> {
    return this.handler.getIsShoppingListEligible();
  }

  getCallbackRouter(): CommerceInternalsPageCallbackRouter {
    return this.callbackRouter;
  }
}

let instance: CommerceInternalsApiProxy|null = null;
