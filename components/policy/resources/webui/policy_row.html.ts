// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyRowElement} from './policy_row.js';

export function getHtml(this: PolicyRowElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="policy row" role="row">
  <div class="name" role="rowheader" aria-labelledby="name">
    <a class="link" target="_blank" href="${this.policy?.link || '#'}" title="${
      this.getLearnMoreTooltip()}">
      <span id="name">${this.policy?.name || ''}</span>
      <img src="chrome://resources/images/open_in_new.svg" alt="">
    </a>
  </div>
  <div class="value" role="cell">${this.getTruncatedValue()}</div>
  <div class="source" role="cell">${this.getSourceText()}</div>
  <div class="scope" role="cell">${this.getScopeText()}</div>
  <div class="level" role="cell">${this.getLevelText()}</div>
  <div class="messages" role="cell">${this.getMessagesText()}</div>
  <div class="toggle" role="cell" @click="${this.onToggleExpandedClick}">
    <cr-icon-button
        iron-icon="${this.expanded ? 'cr:keyboard-arrow-up' : 'cr:keyboard-arrow-down'}"
        aria-label="${this.getShowMoreLessLabel()}">
    </cr-icon-button>
  </div>
</div>
<div class="value row" role="row" ?hidden="${!this.expanded}">
  <div class="name" role="rowheader">$i18n{value}</div>
  <div class="value" role="cell">${this.getFormattedValue()}</div>
  <div class="copy" role="cell">
    <a is="action-link" class="copy-value link" role="button" @click="${
      this.onCopyClick}" title="${this.getCopyLabel()}" aria-label="${
      this.getCopyLabel()}">
      <img src="chrome://resources/images/icon_copy_content.svg"
          alt="" aria-hidden="true">
    </a>
  </div>
</div>
<div class="scope row" role="row" ?hidden="${!this.expanded}">
  <div class="name" role="rowheader">$i18n{headerScope}</div>
  <div class="value" role="cell">${this.getScopeText()}</div>
</div>
<div class="level row" role="row" ?hidden="${!this.expanded}">
  <div class="name" role="rowheader">$i18n{headerLevel}</div>
  <div class="value" role="cell">${this.getLevelText()}</div>
</div>
<div class="messages row" role="row" ?hidden="${!this.expanded}">
  <div class="name" role="rowheader">$i18n{headerStatus}</div>
  <div class="value" role="cell">${this.getMessagesText()}</div>
</div>
<div class="errors row" role="row" ?hidden="${
  !this.expanded || !this.policy?.error}">
  <div class="name" role="rowheader">$i18n{error}</div>
  <div class="value" role="cell">${
      this.policy?.error ||
      ''}</div>
</div>
<div class="warnings row" role="row" ?hidden="${
  !this.expanded || !this.policy?.warning}">
  <div class="name" role="rowheader">$i18n{warning}</div>
  <div class="value" role="cell">${
      this.policy?.warning ||
      ''}</div>
</div>
<div class="infos row" role="row" ?hidden="${
  !this.expanded || !this.policy?.info}">
  <div class="name" role="rowheader">$i18n{info}</div>
  <div class="value" role="cell">${
      this.policy?.info ||
      ''}</div>
</div>
${
      this.conflictItems.map(item => html`
  <policy-conflict class="${item.className}"
      .conflict="${item.conflict}"
      .rowLabel="${item.label}"
      .policyName="${
          this.policy?.name ||
          ''}"
      ?hidden="${!this.expanded}">
  </policy-conflict>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
