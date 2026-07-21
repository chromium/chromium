// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyLogsAppElement} from './policy_logs_app.js';
import {highlightText} from './policy_logs_search.js';

export function getHtml(this: PolicyLogsAppElement) {
  return html`<!--_html_template_start_-->
    <h1>$i18n{logsTitle}</h1>

    <div class="buttons-row">
      <button id="logs-dump" @click="${
      this.onDumpClick}">$i18n{exportLogsJSON}</button>
      <button id="logs-refresh" @click="${
      this.onRefreshClick}">$i18n{refreshLogs}</button>
    </div>

    <h2>$i18n{versionInfoLabel}</h2>

    <table id="version-info" aria-label="Version Information">
      <tbody>
        <tr>
          <td class="label">$i18n{browserName}</td>
          <td id="chrome-version-value" class="version-info-table-data">
            ${this.versionInfo?.version ?? '-'}
          </td>
        </tr>

        <tr>
          <td class="label">$i18n{revision}</td>
          <td id="chrome-revision-value" class="version-info-table-data">
            ${this.versionInfo?.revision ?? '-'}
          </td>
        </tr>

        <tr>
          <td class="label">$i18n{os}</td>
          <td id="os-version-value" class="version-info-table-data">
            ${this.versionInfo?.deviceOs ?? '-'}
          </td>
        </tr>
      </tbody>
    </table>

    <h2>Logs</h2>

    <div class="filter-row">
      <input type="search" id="filter"
          placeholder="$i18n{filterLogs}"
          aria-label="$i18n{filterLogs}"
          .value="${this.filterPattern}"
          @input="${this.onFilterInput}">
      <div id="severity-filters">
        ${['error', 'warning', 'info', 'verbose'].map(severity => html`
          <label>
            <input type="checkbox"
                id="${severity}-checkbox"
                data-severity="${severity}"
                ?checked="${!!this.checkedSeverities[severity.toUpperCase()]}"
                @change="${this.onSeverityCheckboxChange}">
            ${severity.toUpperCase()}
          </label>
        `)}
      </div>
    </div>

    <div id="logs-container" role="grid" aria-label="Logs List">
      ${
      this.getFilteredLogs().map(
          log => html`
          <div role="row" class="log-line">
            <div role="gridcell" class="log-column timestamp">${
              this.getLogTimestamp(log)}</div>
            <div role="gridcell" class="log-column severity">
              ${highlightText(log.logSeverity, this.getFilterWords())}
            </div>
            <div role="gridcell" class="log-column file-and-line">
              <a href="${log.location}" title="${
              log.fileAndLine}" target="_blank">
                <span class="file">
                  ${
              highlightText(
                  this.getLogFileAndLine(log).file, this.getFilterWords())}
                </span>
                ${
              highlightText(
                  ':' + this.getLogFileAndLine(log).line,
                  this.getFilterWords())}
              </a>
            </div>
            <div role="gridcell" class="log-column message">
              ${highlightText(log.message, this.getFilterWords())}
            </div>
          </div>
        `)}
    </div>

    <h2>$i18n{variations}</h2>

    <ul id="active-variations-container" aria-label="Active Variations List">
      ${this.versionInfo?.variations.map(variation => html`
        <li>${variation}</li>
      `) ?? ''}
    </ul>
  <!--_html_template_end_-->`;
}
