// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './eligibility_list.js';
import './subscription_list.js';
import './shared_style.css.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';

export class CommerceInternalsAppElement extends CrLitElement {
  static get is() {
    return 'commerce-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private commerceInternalsApi_: CommerceInternalsApiProxy =
      CommerceInternalsApiProxy.getInstance();

  protected onResetPriceTrackingEmailPreferenceClick_() {
    this.commerceInternalsApi_.resetPriceTrackingEmailPref();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'commerce-internals-app': CommerceInternalsAppElement;
  }
}

customElements.define(
    CommerceInternalsAppElement.is, CommerceInternalsAppElement);
