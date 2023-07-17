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

  setInitialValues(initialValues: {[key: string]: any}) {
    const policyNameInput =
        this.getRequiredElement('.name-select') as HTMLInputElement;
    const policySourceInput =
        this.getRequiredElement('.source-select') as HTMLInputElement;
    const policyLevelInput =
        this.getRequiredElement('.level-select') as HTMLInputElement;
    const policyScopeInput =
        this.getRequiredElement('.target-select') as HTMLInputElement;
    const policyValueInput =
        this.getRequiredElement('.value-input') as HTMLInputElement;

    policyNameInput.value = initialValues['name'];
    policySourceInput.value = initialValues['source'];
    policyLevelInput.value = initialValues['level'];
    policyScopeInput.value = initialValues['scope'];
    policyValueInput.value = initialValues['value'];
  }

  // Function that initializes the policy selection dropdowns and delete
  // button for the current row.
  private initialize() {
    const policyNameDropdown = this.getRequiredElement('.name');

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

  // Class method for returning the value of the given attribute in this row.
  getValue(selector: string): string {
    return this.getRequiredElement<HTMLSelectElement|HTMLInputElement>(selector)
        .value;
  }
}

// Declare the custom element.
declare global {
  interface HTMLElementTagNameMap {
    'policy-test-row': PolicyTestRowElement;
  }
}
customElements.define('policy-test-row', PolicyTestRowElement);
