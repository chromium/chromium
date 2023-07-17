// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './policy_test_row.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {PolicyTestRowElement} from './policy_test_row.js';
import {getTemplate} from './policy_test_table.html.js';

interface PolicyInfo {
  name: string;
  source: number;
  scope: number;
  level: number;
  value: string;
}

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
    this.getRequiredElement('.table').appendChild(
        document.createElement('policy-test-row'));
  }

  // Method for adding a row with the initial values in initialValues.
  addRow(initialValues: {[key: string]: any}) {
    const row = this.getRequiredElement<HTMLElement>('.table').appendChild(
        document.createElement('policy-test-row'));
    row.setInitialValues(initialValues);
  }

  // Class method for creating and returning a JSON string containing the policy
  // names, levels, values, scopes and sources selected in the table.
  getTestPoliciesAsJsonString(): string {
    const policyRowArray: PolicyTestRowElement[] =
        Array.from(this.shadowRoot!.querySelectorAll('policy-test-row'));
    const policyInfoArray: PolicyInfo[] =
        policyRowArray.map((row: PolicyTestRowElement) => ({
                             name: row.getValue('.name'),
                             source: Number.parseInt(row.getValue('.source')),
                             scope: Number.parseInt(row.getValue('.scope')),
                             level: Number.parseInt(row.getValue('.level')),
                             value: row.getValue('.value'),
                           }));
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
