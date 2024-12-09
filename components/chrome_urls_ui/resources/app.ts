// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

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
      debugPagesButtonDisabled_: {type: Boolean},
      internalUrlInfos_: {type: Array},
      webuiUrlInfos_: {type: Array},
      commandUrls_: {type: Array},
      internalUisEnabled_: {type: Boolean},
    };
  }

  protected debugPagesButtonDisabled_: boolean = false;
  protected webuiUrlInfos_: WebuiUrlInfo[] = [];
  protected internalUrlInfos_: WebuiUrlInfo[] = [];
  protected commandUrls_: Url[] = [];
  protected internalUisEnabled_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    BrowserProxyImpl.getInstance().handler.getUrls().then(({urlsData}) => {
      // Since we use GURL on the C++ side, we need to remove the trailing
      // '/' here for nicer display.
      function getPrettyUrl(url: Url): Url {
        return {url: url.url.replace(/\/$/, '')};
      }
      urlsData.webuiUrls.forEach(info => {
        info.url = getPrettyUrl(info.url);
      });
      this.webuiUrlInfos_ = urlsData.webuiUrls.filter(info => !info.internal);
      this.internalUrlInfos_ = urlsData.webuiUrls.filter(info => info.internal);
      this.commandUrls_ = urlsData.commandUrls.map(url => getPrettyUrl(url));
      this.internalUisEnabled_ = urlsData.internalDebuggingUisEnabled;
    });
  }

  protected getDebugPagesEnabledText_(): string {
    return this.internalUisEnabled_ ? 'enabled' : 'disabled';
  }

  protected getDebugPagesToggleButtonLabel_(): string {
    return this.internalUisEnabled_ ? 'Disable internal debugging pages' :
                                      'Enable internal debugging pages';
  }

  protected async onToggleDebugPagesClick_() {
    this.debugPagesButtonDisabled_ = true;
    const enabled = !this.internalUisEnabled_;
    await BrowserProxyImpl.getInstance().handler.setDebugPagesEnabled(enabled);
    this.internalUisEnabled_ = enabled;
    this.debugPagesButtonDisabled_ = false;
  }

  protected isInternalUiEnabled_(info: WebuiUrlInfo): boolean {
    return info.enabled && this.internalUisEnabled_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'chrome-urls-app': ChromeUrlsAppElement;
  }
}

customElements.define(ChromeUrlsAppElement.is, ChromeUrlsAppElement);
