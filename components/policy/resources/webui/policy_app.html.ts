// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyAppElement} from './policy_app.js';

export function getHtml(this: PolicyAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <cr-toolbar page-name="$i18n{title}"
      show-search
      search-prompt="$i18n{filterPlaceholder}"
      clear-label="$i18n{clearSearch}"
      @search-changed="${this.onSearchChanged_}">
  </cr-toolbar>
  <div class="action-header-row">
    <div class="action-row-buttons">
      <cr-button id="reload-policies"
          ?disabled="${this.reloadButtonDisabled_}"
          @click="${this.onReloadPoliciesClick_}">
        $i18n{reloadPolicies}
      </cr-button>
      <div style="position: relative;">
        <cr-button id="more-actions-button" @click="${this.onMoreActionsClick_}">
          $i18n{moreActions}
          <cr-icon icon="cr:expand-more" slot="suffix-icon"></cr-icon>
        </cr-button>
        <cr-action-menu id="actionMenu" role-description="menu">
          ${!this.hideExportButton_ ? html`
            <button id="export-policies" class="dropdown-item"
                @click="${this.onExportPoliciesClick_}">
              $i18n{exportPoliciesJSON}
            </button>
          ` : ''}
          <button id="copy-policies" class="dropdown-item"
              @click="${this.onCopyPoliciesClick_}">
            $i18n{copyPoliciesJSON}
          </button>
<if expr="not is_chromeos">
          <button id="upload-report" class="dropdown-item"
              ?hidden="${!this.enableReportButton_ || this.hideUploadReportButton_}"
              ?disabled="${this.uploadReportButtonDisabled_}"
              @click="${this.onUploadReportClick_}">
            $i18n{uploadReport}
          </button>
          <button id="view-logs" class="dropdown-item"
              @click="${this.onViewLogsClick_}">
            $i18n{viewLogs}
          </button>
</if> <!-- not is_chromeos -->
        </cr-action-menu>
      </div>
    </div>
    <cr-checkbox id="show-unset"
        ?checked="${this.showUnset_}"
        @checked-changed="${this.onShowUnsetCheckedChanged_}">
      $i18n{showUnset}
    </cr-checkbox>
  </div>
  <main id="policy-ui-container">
    <div id="policy-ui">
<if expr="not is_ios and not is_android">
      ${this.shouldShowCommandLineFlagsWarning_ ? html`
        <div id="command-line-flags-warning" class="warning-banner" role="alert">
          <cr-icon icon="cr:warning" class="warning-icon"></cr-icon>
          <div class="warning-text">$i18n{commandLineFlagsWarning}</div>
        </div>
      ` : ''}
      ${this.shouldShowPromo_ ? html`
        <promotion-banner-section-container
            @dismiss="${this.onPromoDismiss_}"
            @redirect="${this.onPromoRedirect_}">
        </promotion-banner-section-container>
      ` : ''}
</if>
      <section id="status-section" class="status-box-section"
          ?hidden="${!this.hasStatus_()}">
        <h2>$i18n{status}</h2>
        <div id="status-box-container">
          ${Object.entries(this.status_).map(([scope, boxStatus]) =>
            boxStatus.policyDescriptionKey ? html`
              <status-box .scope="${scope}" .status="${boxStatus}"></status-box>
            ` : '')}
        </div>
      </section>
      <section id="main-section"
          class="${this.policyGroups_.length === 0 ? 'empty' : ''}">
        ${this.policyGroups_.map(group => html`
          <policy-table
              .dataModel="${group}"
              .filterPattern="${this.filterPattern_}"
              .showUnset="${this.showUnset_}">
          </policy-table>
        `)}
      </section>
    </div>
  </main>
  <div id="toast-container" role="alert" aria-live="polite">
    ${this.toastText_ ? html`<div class="toast">${this.toastText_}</div>` : ''}
  </div>

  <!--_html_template_end_-->`;
  // clang-format on
}
