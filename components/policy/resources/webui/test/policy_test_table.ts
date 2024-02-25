// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './policy_test_row.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {PolicyInfo, PolicySchema} from './policy_test_browser_proxy.js';
import type {PolicyTestRowElement} from './policy_test_row.js';
import {getTemplate} from './policy_test_table.html.js';

export class PolicyTestTableElement extends CustomElement {
  private schema_?: PolicySchema;

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.setSchema(JSON.parse(loadTimeData.getString('initialSchema')));
    this.getRequiredElement('#add-policy-btn')
        .addEventListener('click', this.addEmptyRow.bind(this));
  }

  setSchema(schema: PolicySchema) {
    const hadSchema = !!this.schema_;
    this.schema_ = schema;
    for (const row of this.shadowRoot!.querySelectorAll('policy-test-row')) {
      if (!(row.getNamespace() in schema)) {
        // This was a policy for a now-uninstalled extension, so completely
        // delete the row.
        row.remove();
      } else {
        // Update the namespace dropdown, etc.
        row.setSchema(schema);
      }
    }
    if (!hadSchema && !this.shadowRoot!.querySelector('policy-test-row')) {
      // On startup, add a single empty row.
      this.addEmptyRow();
    }
  }

  clearRows() {
    const table = this.getRequiredElement<HTMLElement>('.table');
    while (table.childElementCount > 1) {
      table.removeChild(table.lastChild!);
    }
  }

  // Event listener function that adds a new PolicyTestRowElement to the table
  // when the Add Policy button is clicked.
  addEmptyRow() {
    assert(this.schema_);
    const newRow = document.createElement('policy-test-row');
    newRow.setSchema(this.schema_);
    // If there is a row before this one, copy its namespace, source, scope,
    // level and preset values.
    const rows = this.shadowRoot!.querySelectorAll('policy-test-row');
    if (rows.length > 0) {
      const lastRow = rows[rows.length - 1];
      const attributesToCopy =
          ['.namespace', '.source', '.scope', '.level', '.preset'];
      attributesToCopy.forEach((attribute: string) => {
        const currSelectElement =
            newRow.getRequiredElement<HTMLSelectElement>(attribute);
        const prevSelectElement =
            lastRow!.getRequiredElement<HTMLSelectElement>(attribute);
        currSelectElement.value = prevSelectElement.value;
        currSelectElement.disabled = prevSelectElement.disabled;
      });
    }
    newRow.updatePolicyNames();
    this.getRequiredElement('.table').appendChild(newRow);
  }

  // Method for adding a row with the initial values in initialValues.
  addRow(initialValues: PolicyInfo) {
    assert(this.schema_);
    const row = this.getRequiredElement<HTMLElement>('.table').appendChild(
        document.createElement('policy-test-row'));
    row.setSchema(this.schema_);
    row.setInitialValues(initialValues);
  }

  // Class method for creating and returning a JSON string containing the policy
  // names, levels, values, scopes and sources selected in the table using the
  // PolicyInfo interface.
  getTestPoliciesJsonString(): string {
    const policyRowArray: PolicyTestRowElement[] =
        Array.from(this.shadowRoot!.querySelectorAll('policy-test-row'));
    const policyInfoArray: PolicyInfo[] = policyRowArray.map(
        (row: PolicyTestRowElement) => ({
          namespace: row.getPolicyNamespace(),
          name: row.getPolicyName(),
          source: Number.parseInt(row.getPolicyAttribute('source')),
          scope: Number.parseInt(row.getPolicyAttribute('scope')),
          level: Number.parseInt(row.getPolicyAttribute('level')),
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
}

// Declare the custom element
declare global {
  interface HTMLElementTagNameMap {
    'policy-test-table': PolicyTestTableElement;
  }
}
customElements.define('policy-test-table', PolicyTestTableElement);
