// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyTableElement} from './policy_table.js';

export function getHtml(this: PolicyTableElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="policy-table" role="table" aria-labelledby="policy-header">
  <h2 class="header" id="policy-header">
    ${this.dataModel?.name || this.dataModel?.id}
    ${this.dataModel?.id === 'updater' ? html`
      <a href="chrome://updater" class="updater-link" target="_blank"
          rel="noopener noreferrer">
        (chrome://updater)
      </a>
    ` : ''}
  </h2>
  <p class="id" ?hidden="${!this.shouldShowId()}">
    ${this.dataModel?.id}
  </p>
  <div class="main">
    <div class="header row" role="row">
      <policy-table-header-cell
        class="name"
        field="name"
        header-title="$i18n{headerName}"
        .sortedColumn="${this.mostRecentSortedColumn}"
        .sortAscending="${this.sortAscending}"
        @sort-changed="${this.onSortChanged}">
      </policy-table-header-cell>
      <policy-table-header-cell
        class="value"
        header-title="$i18n{headerValue}"
        no-sort>
      </policy-table-header-cell>
      <policy-table-header-cell
        class="source"
        field="source"
        header-title="$i18n{headerSource}"
        .sortedColumn="${this.mostRecentSortedColumn}"
        .sortAscending="${this.sortAscending}"
        @sort-changed="${this.onSortChanged}">
      </policy-table-header-cell>
      <policy-table-header-cell
        class="scope"
        field="scope"
        header-title="$i18n{headerScope}"
        .sortedColumn="${this.mostRecentSortedColumn}"
        .sortAscending="${this.sortAscending}"
        @sort-changed="${this.onSortChanged}">
      </policy-table-header-cell>
      <policy-table-header-cell
        class="level"
        field="level"
        header-title="$i18n{headerLevel}"
        .sortedColumn="${this.mostRecentSortedColumn}"
        .sortAscending="${this.sortAscending}"
        @sort-changed="${this.onSortChanged}">
      </policy-table-header-cell>
      <policy-table-header-cell
        class="status"
        field="status"
        header-title="$i18n{headerStatus}"
        .sortedColumn="${this.mostRecentSortedColumn}"
        .sortAscending="${this.sortAscending}"
        @sort-changed="${this.onSortChanged}">
      </policy-table-header-cell>
      <div class="toggle" role="columnheader"></div>
    </div>
    ${this.sortedPolicies.map(policy => html`
      <policy-row
        .policy="${policy}"
        class="policy-data"
        ?hidden="${this.isPolicyHidden(policy)}">
      </policy-row>
    `)}
    <div class="no-policy" ?hidden="${this.hasVisiblePolicies()}">
      $i18n{noPoliciesSet}
    </div>
    ${this.dataModel?.name === 'Policy Precedence' &&
      this.dataModel?.precedenceOrder ? html`
      <policy-precedence-row
          .precedenceOrder="${this.dataModel.precedenceOrder}"
          class="policy-precedence-data">
      </policy-precedence-row>
    ` : ''}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
