// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {CommerceInternalsHandlerFactory, CommerceInternalsHandlerRemote, CommerceInternalsPageCallbackRouter, ShoppingListEligibleDetail, Subscription} from './commerce_internals.mojom-webui.js';
import {ProductInfo} from './shopping_list.mojom-webui.js';

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

  getShoppingListEligibleDetails():
      Promise<{detail: ShoppingListEligibleDetail}> {
    return this.handler.getShoppingListEligibleDetails();
  }

  resetPriceTrackingEmailPref(): void {
    this.handler.resetPriceTrackingEmailPref();
  }

  getProductInfoForUrl(url: Url): Promise<{info: ProductInfo}> {
    return this.handler.getProductInfoForUrl(url);
  }

  getCallbackRouter(): CommerceInternalsPageCallbackRouter {
    return this.callbackRouter;
  }

  getSubscriptionDetails(): Promise<{subscriptions: Subscription[]}> {
    return this.handler.getSubscriptionDetails();
  }
}

let instance: CommerceInternalsApiProxy|null = null;
