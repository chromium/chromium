// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EligibilityListElement} from './eligibility_list.js';

export function getHtml(this: EligibilityListElement) {
  // clang-format off
  return html`
  <button @click="${this.onRefreshDetailsClick_}">
    Refresh eligibility details
  </button>
  <ul>
    <li><b>Country</b>: ${this.country_}</li>
    <li><b>Locale</b>: ${this.locale_}</li>
    ${this.details_.map(detail => html`
      <li>
        <b>${detail.name}</b>: ${detail.value}
        <span class="${this.getColor_(detail)}">${this.getMark_(detail)}</span>
      </li>`)}
  </ul>`;
  // clang-format on
}
