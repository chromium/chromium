// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="header">
  <div class="flex-container">
    <div class="flex search-container">
      <input type="text" id="search"
          aria-label="$i18n{search-label}"
          placeholder="$i18n{search-placeholder}"
          autocomplete="off" spellcheck="false"
          @input="${this.onSearchInput_}">
      <input type="button" class="clear-search" title="$i18n{clear-search}"
          @click="${this.onClearSearchClick_}">
    </div>
    <div class="flex">
      <cr-button id="experiment-reset-all" @click="${this.onResetAllClick_}"
          @keydown="${this.onResetAllKeydown_}" @blur="${this.onResetAllBlur_}">
        $i18n{reset}
      </cr-button>
    </div>
  </div>
  <div class="screen-reader-only" id="screen-reader-status-message"
      role="status"></div>
</div>
<div id="body-container">
  <div id="flagsTemplate">
<if expr="chromeos_lacros or chromeos_ash">
    <div class="os-link-container" id="os-link-container"
          ?hidden="${!this.data.showSystemFlagsLink}">
      <span id="os-flags-link-container" class="os-link-icon"></span>
      <span aria-hidden="true" id="os-link-desc">$i18n{os-flags-text1}</span>
      <a href="#" id="os-link-href" aria-describedby="os-link-desc"
          @click="${this.onOsLinkHrefClick_}">
        $i18n{os-flags-link}
      </a>
      <span aria-hidden="true">$i18n{os-flags-text2}</span>
    </div>
</if>

    <div class="flex-container">
      <div class="flex">
        <h1 class="section-header-title">$i18n{heading}</h1>
      </div>
      <span id="version" class="flex">$i18n{version}</span>
    </div>
    <div class="blurb-container">
      <span class="blurb-warning">$i18n{page-warning}</span>
      <span>$i18n{page-warning-explanation}</span>
<if expr="chromeos_ash">
      <p id="owner-warning" ?hidden="${!this.data.showOwnerWarning}">
        $i18n{owner-warning}
      </p>
</if>
    </div>
    <p id="promos" ?hidden="${!this.shouldShowPromos_()}">
      <!-- Those strings are not localized because they only appear in
          chrome://flags, which is not localized. -->
      <span id="channel-promo-beta"
          ?hidden="${!this.data.showBetaChannelPromotion}">
        Interested in cool new Chrome features? Try our
        <a href="https://chrome.com/beta">beta channel</a>.
      </span>
      <span id="channel-promo-dev"
          ?hidden="${this.data.showDevChannelPromotion}">
        Interested in cool new Chrome features? Try our
        <a href="https://chrome.com/dev">dev channel</a>
      </span>
    </p>

    <cr-tabs id="tabs" .tabNames="${this.tabNames_}"
        .selected="${this.selectedTabIndex_}"
        @selected-changed="${this.onSelectedTabIndexChanged_}">
    </cr-tabs>

    <div id="tabpanels">
      <div id="tab-content-available" class="tab-content"
          ?selected="${this.isTabSelected_(0)}"
          role="tabpanel" aria-labelledby="tab-available" aria-hidden="false">
        <!-- Non default experiments. -->
        <div id="non-default-experiments">
          ${this.nonDefaultFeatures.map(feature => html`
            <flags-experiment id="${feature.internal_name}" .data="${feature}"
                @select-change="${this.onSelectChange_}"
                @textarea-change="${this.onTextareaChange_}"
                @input-change="${this.onInputChange_}">
            </flags-experiment>
          `)}
        </div>
        <!-- Experiments with default settings. -->
        <div id="default-experiments">
          ${this.defaultFeatures.map(feature => html`
            <flags-experiment id="${feature.internal_name}" .data="${feature}"
                @select-change="${this.onSelectChange_}"
                @textarea-change="${this.onTextareaChange_}"
                @input-change="${this.onInputChange_}">
            </flags-experiment>
          `)}
        </div>
        <div class="no-match" role="alert" hidden>$i18n{no-results}</div>
      </div>
<if expr="not is_ios">
      <div id="tab-content-unavailable" class="tab-content"
          ?selected="${this.isTabSelected_(1)}"
          role="tabpanel" aria-labelledby="tab-unavailable" aria-hidden="false">
        <div id="unavailable-experiments">
          ${this.data.unsupportedFeatures.map(feature => html`
            <flags-experiment id="${feature.internal_name}" .data="${feature}"
                unsupported>
            </flags-experiment>
          `)}
        </div>
        <div class="no-match" role="alert" hidden>$i18n{no-results}</div>
      </div>
</if>
    </div>
    <div id="needs-restart">
      <div class="flex-container">
        <div class="flex restart-notice">$i18n{flagsRestartNotice}</div>
        <div class="flex">
<if expr="not is_ios">
          <cr-button id="experiment-restart-button" class="action-button"
              @click="${this.onRestartButtonClick_}"
              @keydown="${this.onRestartButtonKeydown_}"
              @blur="${this.onRestartButtonBlur_}">
            $i18n{relaunch}
          </cr-button>
</if>
        </div>
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
