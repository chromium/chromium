// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ProductInfo} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CommerceInternalsApiProxy} from '../commerce_internals_api_proxy.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface ProductViewerAppElement {
  $: {
    productUrl: HTMLInputElement,
  };
}

export class ProductViewerAppElement extends CrLitElement {
  static get is() {
    return 'product-viewer-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      product_: {type: Object},
    };
  }

  protected product_: ProductInfo|null = null;

  private commerceInternalsApi_: CommerceInternalsApiProxy =
      CommerceInternalsApiProxy.getInstance();

  protected async loadProduct_() {
    const productInfo = (await this.commerceInternalsApi_.getProductInfoForUrl({
                          url: this.$.productUrl.value,
                        })).info;
    if (!productInfo) {
      return;
    }

    this.product_ = productInfo;
  }
}

customElements.define(ProductViewerAppElement.is, ProductViewerAppElement);
