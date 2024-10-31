// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ProductSpecificationsSet} from './commerce_internals.mojom-webui.js';
import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';
import {getHtml} from './product_specifications_set_list.html.js';
import {getCss} from './shared_style.css.js';

export class ProductSpecificationsTableListElement extends CrLitElement {
  static get is() {
    return 'product-specifications-set-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      sets_: {type: Array},
    };
  }

  protected sets_: ProductSpecificationsSet[] = [];

  private commerceInternalsApi_: CommerceInternalsApiProxy =
      CommerceInternalsApiProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.loadProductSpecificationsSets_();
  }

  protected async loadProductSpecificationsSets_() {
    const productSpecsSets =
        (await this.commerceInternalsApi_.getProductSpecificationsDetails())
            .productSpecificationsSet;
    if (!productSpecsSets || productSpecsSets.length === 0) {
      this.sets_ = [];
      return;
    }

    this.sets_ = productSpecsSets;
  }
}

customElements.define(
    ProductSpecificationsTableListElement.is,
    ProductSpecificationsTableListElement);
