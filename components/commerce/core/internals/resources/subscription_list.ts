// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Subscription} from './commerce_internals.mojom-webui.js';
import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';
import {getCss} from './shared_style.css.js';
import {getHtml} from './subscription_list.html.js';

export class SubscriptionListElement extends CrLitElement {
  static get is() {
    return 'subscription-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      subscriptions_: {type: Array},
    };
  }

  protected subscriptions_: Subscription[] = [];

  private commerceInternalsApi_: CommerceInternalsApiProxy =
      CommerceInternalsApiProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.loadSubscriptions_();
  }

  protected async loadSubscriptions_() {
    const subscriptions =
        (await this.commerceInternalsApi_.getSubscriptionDetails())
            .subscriptions;
    if (!subscriptions || subscriptions.length === 0) {
      this.subscriptions_ = [];
      return;
    }

    this.subscriptions_ = subscriptions;
  }
}

customElements.define(SubscriptionListElement.is, SubscriptionListElement);
