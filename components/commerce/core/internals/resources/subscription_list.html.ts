// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SubscriptionListElement} from './subscription_list.js';

export function getHtml(this: SubscriptionListElement) {
  // clang-format off
  return this.subscriptions_.length > 0 ? html`
  <table class="list">
    <thead>
      <tr>
        <th>Cluster ID</th>
        <th>Domain</th>
        <th>Current Price</th>
        <th>Previous Price</th>
        <th>Product</th>
      </tr>
    </thead>
    <tbody>
      ${this.subscriptions_.map(subscription =>
        subscription.productInfos.length === 0 ? html`
          <tr>
            <td>${BigInt(subscription.clusterId)}</td>
          </tr>` : subscription.productInfos.map(productInfo => html`
          <tr>
            <td>${BigInt(productInfo.info.clusterId)}</td>
            <td>${productInfo.info.domain}</td>
            <td>${productInfo.info.currentPrice}</td>
            <td>${productInfo.info.previousPrice}</td>
            <td>
              ${productInfo.info.productUrl.url ? html`
                <a href="${productInfo.info.productUrl.url}" target="_blank">
                  ${productInfo.info.title}
                </a>` : productInfo.info.title}
              ${productInfo.info.imageUrl && html`
                <a href="${productInfo.info.imageUrl.url}" target="_blank">
                  (image)
                </a>`}
            </td>
          </tr>`))}
    </tbody>
  </table>` : html`<div>No subscriptions found.</div>`;
  // clang-format on
}
