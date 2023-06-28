// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './policy_test_row.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './policy_test_table.html.js';

export class PolicyTestTableElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.getRequiredElement('#add-policy-btn')
        .addEventListener('click', this.addRow.bind(this));
  }

  // Event listener function that adds a new PolicyTestRowElement to the table
  // when the Add Policy button is clicked
  private addRow() {
    this.getRequiredElement<HTMLElement>('.table').appendChild(
        document.createElement('policy-test-row'));
  }
}

// Declare the custom element
declare global {
  interface HTMLElementTagNameMap {
    'policy-test-table': PolicyTestTableElement;
  }
}
customElements.define('policy-test-table', PolicyTestTableElement);
