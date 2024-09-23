// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProductInfo} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import type {ProductSpecificationsSet, ShoppingListEligibleDetail, Subscription} from './commerce_internals.mojom-webui.js';
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

  getProductSpecificationsDetails():
      Promise<{productSpecificationsSet: ProductSpecificationsSet[]}> {
    return this.handler.getProductSpecificationsDetails();
  }

  resetProductSpecifications(): void {
    return this.handler.resetProductSpecifications();
  }
}

let instance: CommerceInternalsApiProxy|null = null;
