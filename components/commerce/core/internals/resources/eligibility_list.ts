// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {EligibilityDetail} from './commerce_internals.mojom-webui.js';
import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';
import {getCss} from './eligibility_list.css.js';
import {getHtml} from './eligibility_list.html.js';

export class EligibilityListElement extends CrLitElement {
  static get is() {
    return 'commerce-internals-eligibility-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      country_: {
        type: String,
      },
      details_: {
        type: Array,
      },
      locale_: {
        type: String,
      },
    };
  }

  protected country_: string = '';
  protected details_: EligibilityDetail[] = [];
  protected locale_: string = '';

  private commerceInternalsApi_: CommerceInternalsApiProxy =
      CommerceInternalsApiProxy.getInstance();

  constructor() {
    super();

    this.commerceInternalsApi_.getCallbackRouter()
        .onShoppingListEligibilityChanged.addListener(() => {
          this.refreshDetails_();
        });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.refreshDetails_();
  }

  protected async refreshDetails_() {
    const details =
        (await this.commerceInternalsApi_.getShoppingEligibilityDetails())
            .details;

    this.country_ = details.country;
    this.locale_ = details.locale;
    this.details_ = details.details;
  }
}

customElements.define(EligibilityListElement.is, EligibilityListElement);
