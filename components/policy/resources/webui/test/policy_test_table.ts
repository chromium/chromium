// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './policy_test_row.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {PolicyInfo} from './policy_test_browser_proxy.js';
import {PolicyTestRowElement} from './policy_test_row.js';
import {getTemplate} from './policy_test_table.html.js';

export class PolicyTestTableElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.getRequiredElement('#add-policy-btn')
        .addEventListener('click', this.addEmptyRow.bind(this));
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
    const newRow = document.createElement('policy-test-row');
    // If there is a row before this one, copy its source, scope, level and
    // preset values.
    const rows = this.shadowRoot!.querySelectorAll('policy-test-row');
    if (rows.length > 0) {
      const lastRow = rows[rows.length - 1];
      const attributesToCopy = ['.source', '.scope', '.level', '.preset'];
      attributesToCopy.forEach((attribute: string) => {
        const currSelectElement =
            newRow.getRequiredElement<HTMLSelectElement>(attribute);
        const prevSelectElement =
            lastRow!.getRequiredElement<HTMLSelectElement>(attribute);
        currSelectElement.value = prevSelectElement.value;
        currSelectElement.disabled = prevSelectElement.disabled;
      });
    }
    this.getRequiredElement('.table').appendChild(newRow);
  }

  // Method for adding a row with the initial values in initialValues.
  addRow(initialValues: PolicyInfo) {
    const row = this.getRequiredElement<HTMLElement>('.table').appendChild(
        document.createElement('policy-test-row'));
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
