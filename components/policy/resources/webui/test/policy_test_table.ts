// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './policy_test_row.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PolicyLevel, PolicyScope, PolicySource} from './policy_test_browser_proxy.js';
import type {PolicyInfo, PolicySchema} from './policy_test_browser_proxy.js';
import type {PolicyTestRowElement} from './policy_test_row.js';
import {getCss} from './policy_test_table.css.js';
import {getHtml} from './policy_test_table.html.js';

export class PolicyTestTableElement extends CrLitElement {
  static get is() {
    return 'policy-test-table';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      schema: {type: Object},
      rows: {type: Array},
    };
  }

  accessor schema: PolicySchema|null = null;
  accessor rows: PolicyInfo[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.setSchema(JSON.parse(loadTimeData.getString('initialSchema')));
  }

  setSchema(schema: PolicySchema) {
    const hadSchema = !!this.schema;
    this.schema = schema;

    // Filter out rows for now-uninstalled extensions.
    const rowEls = this.shadowRoot ?
        Array.from(this.shadowRoot.querySelectorAll('policy-test-row')) :
        [];
    const updatedRows: PolicyInfo[] = [];
    rowEls.forEach(row => {
      // Read properties directly instead of calling getters (like
      // getPolicyName) to avoid triggering validation side effects (which
      // highlight empty rows in red) during state synchronization.
      const ns = row.policyNamespace;
      if (ns in schema) {
        updatedRows.push({
          namespace: ns,
          name: row.policyName,
          source: row.policySource,
          scope: row.policyScope,
          level: row.policyLevel,
          value: row.getPolicyValue(),
        });
      }
    });

    if (updatedRows.length > 0 || rowEls.length > 0) {
      this.rows = updatedRows;
    }

    if (!hadSchema && this.rows.length === 0) {
      this.addEmptyRow();
    }
  }

  clearRows() {
    this.rows = [];
  }

  addEmptyRow() {
    assert(this.schema);
    let newRowValues: PolicyInfo = {
      namespace: 'chrome',
      name: '',
      value: '',
      source: PolicySource.SOURCE_ENTERPRISE_DEFAULT_VAL,
      scope: PolicyScope.SCOPE_USER_VAL,
      level: PolicyLevel.LEVEL_MANDATORY_VAL,
    };

    const rowEls = this.shadowRoot ?
        Array.from(this.shadowRoot.querySelectorAll('policy-test-row')) :
        [];
    if (rowEls.length > 0) {
      const lastRowEl = rowEls[rowEls.length - 1]!;
      newRowValues = {
        namespace: lastRowEl.getPolicyNamespace(),
        name: '',
        value: '',
        source: Number(lastRowEl.getPolicyAttribute('source')),
        scope: Number(lastRowEl.getPolicyAttribute('scope')),
        level: Number(lastRowEl.getPolicyAttribute('level')),
      };
    }
    this.rows = [...this.rows, newRowValues];
  }

  addRow(initialValues: PolicyInfo) {
    assert(this.schema);
    this.rows = [...this.rows, initialValues];
  }

  getTestPoliciesJsonString(): string {
    const policyRowArray = this.shadowRoot ?
        Array.from(this.shadowRoot.querySelectorAll('policy-test-row')) :
        [];
    const policyInfoArray: PolicyInfo[] =
        policyRowArray.map(row => ({
                             namespace: row.getPolicyNamespace(),
                             name: row.getPolicyName(),
                             source: Number(row.getPolicyAttribute('source')),
                             scope: Number(row.getPolicyAttribute('scope')),
                             level: Number(row.getPolicyAttribute('level')),
                             value: row.getPolicyValue(),
                           }));
    // If there is an error anywhere in the table, no policies should be
    // applied.
    const rowHasError = (row: PolicyTestRowElement) => row.getErrorState();
    if (policyRowArray.some(rowHasError)) {
      return '';
    }
    return JSON.stringify(policyInfoArray);
  }

  protected onRemoveRow(e: Event) {
    const rowEl = e.target as HTMLElement;
    const index = Number(rowEl.dataset['index']);
    const rowEls =
        Array.from(this.shadowRoot.querySelectorAll('policy-test-row'));
    // Read properties directly to avoid validation side effects.
    this.rows = rowEls.map(row => ({
                             namespace: row.policyNamespace,
                             name: row.policyName,
                             source: row.policySource,
                             scope: row.policyScope,
                             level: row.policyLevel,
                             value: row.getPolicyValue(),
                           }));
    this.rows.splice(index, 1);
    this.requestUpdate();
  }

  protected onAddPolicyClick() {
    this.addEmptyRow();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-test-table': PolicyTestTableElement;
  }
}
customElements.define(PolicyTestTableElement.is, PolicyTestTableElement);
