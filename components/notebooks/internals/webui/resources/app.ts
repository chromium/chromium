// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {FeatureFlagState, ProfileEligibility} from './notebooks_internals.mojom-webui.js';
import {browserProxyFactory} from './notebooks_internals.mojom-webui.js';

export interface NotebooksInternalsAppElement {
  $: {
    eligibilityTable: HTMLTableElement,
    configTable: HTMLTableElement,
  };
}

export class NotebooksInternalsAppElement extends CrLitElement {
  static get is() {
    return 'notebooks-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      featureFlags: {type: Object},
      eligibility: {type: Object},
    };
  }

  accessor featureFlags: FeatureFlagState|null = null;
  accessor eligibility: ProfileEligibility|null = null;

  override async connectedCallback() {
    super.connectedCallback();

    const browserProxy = browserProxyFactory.getInstance();

    const {flags} = await browserProxy.handler.getFeatureFlagState();
    const {eligibility} = await browserProxy.handler.getProfileEligibility();
    this.featureFlags = flags;
    this.eligibility = eligibility;

    browserProxy.callbackRouter.onProfileEligibilityChanged.addListener(
        this.onEligibilityChanged.bind(this));
  }

  private onEligibilityChanged(eligibility: ProfileEligibility) {
    this.eligibility = eligibility;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'notebooks-internals-app': NotebooksInternalsAppElement;
  }
}

customElements.define(
    NotebooksInternalsAppElement.is, NotebooksInternalsAppElement);
