// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EligibilityListElement} from './eligibility_list.js';

const CHECKMARK_HTML = html`<span class="green">&#10004;</span>`;
const CROSSMARK_HTML = html`<span class="red">&#10006;</span>`;

export function getHtml(this: EligibilityListElement) {
  // clang-format off
  return html`
  <button @click="${this.refreshDetails_}">Refresh eligibility details</button>
  <ul>
    <li><b>Country</b>: ${this.country_}</li>
    <li><b>Locale</b>: ${this.locale_}</li>
    ${this.details_.map(detail => html`
      <li>
        <b>${detail.name}</b>: ${detail.value}
        ${detail.value === detail.expectedValue ? CHECKMARK_HTML :
            CROSSMARK_HTML}
      </li>`)}
  </ul>`;
  // clang-format on
}
