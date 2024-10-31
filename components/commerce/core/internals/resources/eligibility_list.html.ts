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
  <span>Shopping list eligible?
    ${this.details_.every(detail => detail.value === detail.expectedValue) ?
          CHECKMARK_HTML : CROSSMARK_HTML}</span>
  <button @click="${this.refreshDetails_}">Refresh</button>
  <ul>
    ${this.details_.map(detail => html`
      <li>
        ${detail.name}: ${detail.value}
        ${detail.value === detail.expectedValue ? CHECKMARK_HTML :
            CROSSMARK_HTML}
      </li>`)}
  </ul>`;
  // clang-format on
}
