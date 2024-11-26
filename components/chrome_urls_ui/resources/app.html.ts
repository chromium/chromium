// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ChromeUrlsAppElement} from './app.js';

export function getHtml(this: ChromeUrlsAppElement) {
  // clang-format off
  return html`
<h1>List of Chrome URLs</h1>
<ul>
  ${this.webuiUrlInfos_.map(info => html`
    ${info.enabled ?
      html`<li><a href="${info.url.url}">${info.url.url}</a></li>` :
      html`<li>${info.url.url}</li>`
    }`)}
</ul>
<h1>For Debug</h1>
<p>
The following pages are for debugging purposes only. Because they crash or hang
the renderer, they're not linked directly; you can type them into the address
bar if you need them.
</p>
<ul>
  ${this.commandUrls_.map(url => html`<li>${url.url}</li>`)}
</ul>`;
}
