// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProductViewerAppElement} from './app.js';

export function getHtml(this: ProductViewerAppElement) {
  // clang-format off
  return html`
  <h1>Product URL Viewer</h1>
  <div>
    To see information here, first visit the target URL in a normal tab.
  </div>
  <div class="url-input">
    <span>Product URL: </span>
    <input id="productUrl" type="text">
    <button @click="${this.loadProduct_}">Load product</button>
  </div>
  <div>
    ${this.product_ ? html`
      <table>
        <tr>
          <td>Title: </td>
          <td>${this.product_.title}</td>
        </tr>
        <tr>
          <td>Cluster Title: </td>
          <td>${this.product_.clusterTitle}</td>
        </tr>
        <tr>
          <td>Cluster ID: </td>
          <td>${BigInt.asUintN(64, this.product_.clusterId)}</td>
        </tr>
        <tr>
          <td>Image URL: </td>
          <td><a href="${this.product_.imageUrl.url}" target="_blank">${
                            this.product_.imageUrl.url}</a></td>
        </tr>
        <tr>
          <td>Price: </td>
          <td>${this.product_.currentPrice}</td>
        </tr>
        <tr>
          <td>Product Categories: </td>
          <td>${this.product_.categoryLabels.join(', \n')}</td>
        </tr>
      </table>` : html`<span>No product info found.</span>`}
  </div>`;
  // clang-format on
}
