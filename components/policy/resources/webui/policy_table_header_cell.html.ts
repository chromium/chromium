// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyTableHeaderCellElement} from './policy_table_header_cell.js';

export function getHtml(this: PolicyTableHeaderCellElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    ${this.headerTitle}
    ${!this.noSort ? html`
      <div class="sort-arrows">
        <button
          class="sort-up-arrow"
          id="${this.field}-sort-up"
          role="button"
          aria-hidden="false"
          title="$i18n{sort} ${this.headerTitle} $i18n{sortAscending}"
          ?active="${this.sortedColumn === this.field &&
                     this.sortAscending}"
          @click="${this.onSortUpClick}">
        </button>
        <button
          class="sort-down-arrow"
          id="${this.field}-sort-down"
          role="button"
          aria-hidden="false"
          title="$i18n{sort} ${this.headerTitle} $i18n{sortDescending}"
          ?active="${this.sortedColumn === this.field &&
                     !this.sortAscending}"
          @click="${this.onSortDownClick}">
        </button>
      </div>
    ` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
