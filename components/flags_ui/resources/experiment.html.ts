// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExperimentElement} from './experiment.js';

export function getHtml(this: ExperimentElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="experiment" id="${this.feature_.internal_name}">
  <div class="flex-container">
    <div class="flex">
      ${this.showingSearchHit_? html`
        <h2 class="experiment-name clone" id="${this.getHeaderId_()}"
            .title="${this.getExperimentTitle_()}"
            @click="${this.onExperimentNameClick_}">
          <!-- intentionally empty -->
        </h2>
        <p class="body clone" part="body">
          <span class="description"><!-- intentionally empty --></span> –
          <span class="platforms"><!-- intentionally empty --></span>
        </p>
      ` : html`
        <h2 class="experiment-name" id="${this.getHeaderId_()}"
            .title="${this.getExperimentTitle_()}"
            @click="${this.onExperimentNameClick_}">
          ${this.feature_.name}
        </h2>
        <p class="body" part="body">
          <span class="description">${this.feature_.description}</span> –
          <span class="platforms">${this.getPlatforms_()}</span>
        </p>
      `}

      ${this.feature_.origin_list_value !== undefined ? html`
        <div class="textarea-container">
          <textarea class="experiment-origin-list-value"
              aria-labelledby="${this.getHeaderId_()}"
              data-internal-name="${this.feature_.internal_name}"
              .value="${this.feature_.origin_list_value}"
              @change="${this.onTextareaChange_}">
          </textarea>
        </div>
      ` : ''}

      ${this.feature_.string_value !== undefined ? html`
        <div class="input-container">
          <input type="text"
              aria-labelledby="${this.getHeaderId_()}"
              data-internal-name="${this.feature_.internal_name}"
              .value="${this.feature_.string_value}"
              @change="${this.onTextInputChange_}">
          </textarea>
        </div>
      ` : ''}

      ${this.feature_.links ? html`
        <div class="links-container">
          ${this.feature_.links!.map(link => html`
            <a href="${link}">${link}</a>
          `)}
        </div>
      ` : ''}

      ${this.showingSearchHit_? html`
        <a class="permalink clone" href="#${this.feature_.internal_name}">
          <!-- intentionally empty -->
        </a>
      `: html`
        <a class="permalink" href="#${this.feature_.internal_name}">
          #${this.feature_.internal_name}
        </a>
      `}
    </div>
    <div class="flex experiment-actions">
      ${this.unsupported ? html`
        <div id="unsupported">$i18n{not-available-platform}</div>
      ` : ''}

      ${this.showMultiValueSelect_() ? html`
        <select class="experiment-select"
            data-internal-name="${this.feature_.internal_name}"
            aria-labelledby="${this.getHeaderId_()}"
            @change="${this.onExperimentSelectChange_}">
          ${this.feature_.options!.map(option => html`
            <option ?selected="${option.selected}">
              ${option.description}
            </option>
          `)}
        </select>
      ` : ''}

      ${this.showEnableDisableSelect_() ? html`
        <select class="experiment-enable-disable"
            data-internal-name="${this.feature_.internal_name}"
            aria-labelledby="${this.getHeaderId_()}"
            @change="${this.onExperimentEnableDisableChange_}">
          <option value="disabled" ?selected="${!this.feature_.enabled}">
            $i18n{disabled}
          </option>
          <option value="enabled" ?selected="${this.feature_.enabled}">
            $i18n{enabled}
          </option>
        </select>
      ` : ''}
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
