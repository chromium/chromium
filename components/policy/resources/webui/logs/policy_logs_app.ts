// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './../browser_proxy.js';
import type {Log} from './../policy.mojom-webui.js';
import {getCss} from './policy_logs_app.css.js';
import {getHtml} from './policy_logs_app.html.js';
import type {VersionInfo} from './types.js';

export class PolicyLogsAppElement extends CrLitElement {
  static get is() {
    return 'policy-logs-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      logs: {type: Array},
      versionInfo: {type: Object},
      filterPattern: {type: String},
      checkedSeverities: {type: Object},
    };
  }

  accessor logs: Log[] = [];
  accessor versionInfo: VersionInfo|null = null;
  accessor filterPattern: string = '';
  accessor checkedSeverities: Record<string, boolean> = {
    ERROR: true,
    WARNING: true,
    INFO: true,
    VERBOSE: true,
  };

  private policyPageMojoMigrationEnabled =
      loadTimeData.getBoolean('policyPageMojoMigrationEnabled');

  override connectedCallback() {
    super.connectedCallback();
    this.displayVersionInfo();
    this.fetchLogsAndDisplay();
  }

  private displayVersionInfo() {
    this.versionInfo = JSON.parse(loadTimeData.getString('versionInfo'));
  }

  private async fetchLogs() {
    if (this.policyPageMojoMigrationEnabled) {
      this.logs =
          (await BrowserProxy.getInstance().handler.getPolicyLogs()).policyLogs;
    } else {
      this.logs = await sendWithPromise('getPolicyLogs');
    }
  }

  protected fetchLogsAndDisplay() {
    this.fetchLogs();
  }

  protected onDumpClick() {
    const dumpObject = {versionInfo: this.versionInfo, logs: this.logs};
    const data = JSON.stringify(dumpObject, null, 3);
    const filename = 'policy_logs_dump.json';
    const blob = new Blob([data], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.setAttribute('href', url);
    a.setAttribute('download', filename);
    a.click();
  }

  protected onRefreshClick() {
    this.fetchLogsAndDisplay();
  }

  protected onFilterInput(e: Event) {
    this.filterPattern = (e.target as HTMLInputElement).value;
  }

  protected onSeverityChange(severity: string, checked: boolean) {
    this.checkedSeverities = {
      ...this.checkedSeverities,
      [severity]: checked,
    };
  }

  protected onSeverityCheckboxChange(e: Event) {
    const target = e.target as HTMLInputElement;
    const severity = target.dataset['severity']!;
    this.onSeverityChange(severity.toUpperCase(), target.checked);
  }

  protected getFilterWords(): string[] {
    return this.filterPattern.toLowerCase().split(/\s+/).filter(
        t => t.length > 0);
  }

  protected getFilteredLogs(): Log[] {
    const filterWords = this.getFilterWords();
    return this.logs.filter(log => {
      const messageLower = log.message.toLowerCase();
      const fileAndLineLower = log.fileAndLine.toLowerCase();
      const logSeverityLower = log.logSeverity.toLowerCase();
      const matchesFilter = filterWords.length === 0 ||
          filterWords.every(
              word => messageLower.includes(word) ||
                  fileAndLineLower.includes(word) ||
                  logSeverityLower.includes(word));
      const matchesSeverity = this.checkedSeverities[log.logSeverity] ?? true;
      return matchesFilter && matchesSeverity;
    });
  }

  protected getLogTimestamp(log: Log): string {
    return new Date(Number(log.timestamp)).toLocaleString('en-CA', {
      timeZoneName: 'short',
      hour12: false,
    });
  }

  protected getLogFileAndLine(log: Log): {file: string, line: string} {
    const parts = log.fileAndLine.split(':');
    return {
      file: parts[0] ?? '',
      line: parts[1] ?? '',
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-logs-app': PolicyLogsAppElement;
  }
}

customElements.define(PolicyLogsAppElement.is, PolicyLogsAppElement);
