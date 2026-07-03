// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyConflictElement} from './policy_conflict.js';

export function getHtml(this: PolicyConflictElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="policy conflict row" role="row">
  <div class="name" role="rowheader">${this.getRowLabelText()}</div>
  <div class="value" role="cell">${this.getFormattedValue()}</div>
  <div class="source" role="cell">${this.getSourceText()}</div>
  <div class="scope" role="cell">${this.getScopeText()}</div>
  <div class="level" role="cell">${this.getLevelText()}</div>
  <div class="messages" role="cell"></div>
  <div class="copy" role="cell">
    <a is="action-link" class="copy-value link" role="button"
        @click="${this.onCopyClick}"
        title="${this.getCopyLabel()}"
        aria-label="${this.getCopyLabel()}">
      <img src="chrome://resources/images/icon_copy_content.svg"
          alt="" aria-hidden="true">
    </a>
  </div>
</div>
<div class="policy conflict entry" role="rowgroup">
  <div class="value row" role="row">
    <div class="name" role="rowheader">${this.getRowLabelText()}</div>
    <div class="value" role="cell">${this.getFormattedValue()}</div>
    <div class="copy" role="cell">
      <a is="action-link" class="copy-value link" role="button"
          @click="${this.onCopyClick}"
          title="${this.getCopyLabel()}"
          aria-label="${this.getCopyLabel()}">
        <img src="chrome://resources/images/icon_copy_content.svg"
            alt="" aria-hidden="true">
      </a>
    </div>
  </div>
  <div class="source row" role="row">
    <div class="name" role="rowheader">$i18n{headerSource}</div>
    <div class="value" role="cell">${this.getSourceText()}</div>
  </div>
  <div class="scope row" role="row">
    <div class="name" role="rowheader">$i18n{headerScope}</div>
    <div class="value" role="cell">${this.getScopeText()}</div>
  </div>
  <div class="level row" role="row">
    <div class="name" role="rowheader">$i18n{headerLevel}</div>
    <div class="value" role="cell">${this.getLevelText()}</div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
