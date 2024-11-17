// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProductSpecificationsTableListElement} from './product_specifications_set_list.js';

export function getHtml(this: ProductSpecificationsTableListElement) {
  // clang-format off
  return this.sets_.length > 0 ? html`
  <table class="list">
    <thead>
      <tr>
        <th>ID</th>
        <th>Creation Time</th>
        <th>Update Time</th>
        <th>Name</th>
        <th>URLs</th>
      </tr>
    </thead>
    <tbody>
      ${this.sets_.map(set => html`
        <tr>
          <td>${set.uuid}</td>
          <td>${set.creationTime}</td>
          <td>${set.updateTime}</td>
          <td>${set.name}</td>
          <td>
            <ul>
              ${set.urlInfos.map(urlInfo => html`
                <li>
                  <a href="${urlInfo.url.url}" target="_blank">
                    ${urlInfo.title || urlInfo.url.url}
                  </a>
                </li>`)}
            </ul>
          </td>
        </tr>`)}
    </tbody>
  </table>` : html`<div>No product specifications sets found.</div>`;
  // clang-format on
}
