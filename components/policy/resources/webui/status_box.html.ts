// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {StatusBoxElement} from './status_box.js';

export function getHtml(this: StatusBoxElement) {
  return html`<!--_html_template_start_-->
<div class="status-box-fields">
  <h3 class="status-box-heading">${this.getHeading()}</h3>

  ${this.getGeneralFields().map(item => html`
    <div class="status-entry" ?hidden="${!item.show}">
      <div class="label">${item.label}</div>
      <div class="${item.className} status-entry-value">${item.value}</div>
    </div>
  `)}

  ${this.showFlexOrgWarning() ? html`
    <div class="status-entry">
      <div class="label">$i18n{labelWarning}:</div>
      <div class="warning status-entry-value" .innerHTML="${this.getWarning()}">
      </div>
    </div>
  ` : ''}

  ${this.showPolicyFetchSection() ? html`
    <hr class="policy-fetch-separator">
    <h3 class="status-box-heading policy-fetch-heading">
      $i18n{labelPolicyFetch}
    </h3>
  ` : ''}

  ${this.getPolicyFetchFields().map(item => html`
    <div class="status-entry" ?hidden="${!item.show}">
      <div class="label">${item.label}</div>
      <div class="${item.className} status-entry-value">${item.value}</div>
    </div>
  `)}

  ${this.showExtensionInstallSection() ? html`
    <hr class="extension-install-separator">
    <h3 class="status-box-heading extension-install-heading">
      $i18n{labelExtensionInstallPolicyFetch}
    </h3>
  ` : ''}

  ${this.getExtensionInstallFields().map(item => html`
    <div class="status-entry" ?hidden="${!item.show}">
      <div class="label">${item.label}</div>
      <div class="${item.className} status-entry-value">${item.value}</div>
    </div>
  `)}
</div>
<!--_html_template_end_-->`;
}
