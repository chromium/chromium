// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {WebuiUrlInfo} from './chrome_urls.mojom-webui.js';

export class ChromeUrlsAppElement extends CrLitElement {
  static get is() {
    return 'chrome-urls-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      webuiUrlInfos_: {type: Array},
      commandUrls_: {type: Array},
    };
  }

  protected webuiUrlInfos_: WebuiUrlInfo[] = [];
  protected commandUrls_: Url[] = [];

  override connectedCallback() {
    super.connectedCallback();

    BrowserProxyImpl.getInstance().handler.getUrls().then(({urlsData}) => {
      this.webuiUrlInfos_ = urlsData.webuiUrls;
      this.commandUrls_ = urlsData.commandUrls;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-urls-app': ChromeUrlsAppElement;
  }
}

customElements.define(ChromeUrlsAppElement.is, ChromeUrlsAppElement);
