// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyTestTableElement} from './policy_test_table.js';

export function getHtml(this: PolicyTestTableElement) {
  return html`<!--_html_template_start_-->
    <div class="table" role="table">
      <header role="row">
        <div class="cell" id="namespace-header">$i18n{testTableNamespace}</div>
        <div class="cell">$i18n{testTableName}</div>
        <div class="cell">$i18n{testTableValue}</div>
        <div class="cell">$i18n{testTablePreset}</div>
        <div class="cell">$i18n{testTableSource}</div>
        <div class="cell">$i18n{testTableScope}</div>
        <div class="cell">$i18n{testTableLevel}</div>
        <div class="cell" id="header-remove-btn-cell"></div>
      </header>
      ${this.rows.map((row, index) => html`
        <policy-test-row
            .schema="${this.schema}"
            .initialValues="${row}"
            data-index="${index}"
            @remove-row="${this.onRemoveRow}">
        </policy-test-row>
      `)}
    </div>
    <button id="add-policy-btn" @click="${this.onAddPolicyClick}">+</button>
  <!--_html_template_end_-->`;
}
