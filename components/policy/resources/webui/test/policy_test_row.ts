// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './policy_test_row.html.js';

export class PolicyTestRowElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.initialize();
  }

  // Function that initializes the policy selection dropdowns and delete
  // button for the current row.
  private initialize() {
    const policyNameDropdown = this.getRequiredElement('.name-select');

    // Populate the policy name dropdown with all policy names.
    loadTimeData.getString('policyNames')
        .split(',')
        .forEach(function(policyName) {
          const currOpt = document.createElement('option');
          currOpt.textContent = policyName;
          policyNameDropdown.appendChild(currOpt);
        });

    // Add an event listener for this row's delete button.
    this.getRequiredElement('.remove-btn')
        .addEventListener('click', this.remove.bind(this));
  }
}

// Declare the custom element.
declare global {
  interface HTMLElementTagNameMap {
    'policy-test-row': PolicyTestRowElement;
  }
}
customElements.define('policy-test-row', PolicyTestRowElement);
