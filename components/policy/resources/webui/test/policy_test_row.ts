// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './policy_test_row.html.js';

export class PolicyTestRowElement extends CustomElement {
  private hasAnError_: boolean = false;
  private errorEvents_: EventTracker = new EventTracker();

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.initialize();
  }

  getErrorState(): boolean {
    return this.hasAnError_;
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

  // Event listener function for setting the select element background back to
  // white after being highlighted in red, and then clicked by the user.
  private resetErrorState(event: Event) {
    (event.target! as HTMLElement).classList.remove('error');
    this.errorEvents_.remove(event.target!);
    this.hasAnError_ = false;
  }

  // Class method for returning the value for this policy (the value in the
  // value cell of this row).
  getPolicyValue(): string {
    return this.getRequiredElement<HTMLInputElement>('.value').value;
  }

  // Class method for returning the name, level, source or scope set in this
  // row.
  getPolicyAttribute(attributeName: string): string {
    const inputElement: HTMLSelectElement =
        this.getRequiredElement<HTMLSelectElement>(`.${attributeName}`);
    if (inputElement.options[inputElement.selectedIndex]!.hidden) {
      inputElement.classList.add('error');
      this.errorEvents_.add(
          inputElement, 'focus', this.resetErrorState.bind(this));
      this.hasAnError_ = true;
    }
    return inputElement.value;
  }
}

// Declare the custom element.
declare global {
  interface HTMLElementTagNameMap {
    'policy-test-row': PolicyTestRowElement;
  }
}
customElements.define('policy-test-row', PolicyTestRowElement);
