// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {PolicyInfo, PolicyLevel, PolicyScope, PolicySource} from './policy_test.js';
import {getTemplate} from './policy_test_row.html.js';

export class PolicyTestRowElement extends CustomElement {
  private hasAnError_: boolean = false;
  private errorEvents_: EventTracker = new EventTracker();
  private inputType_: string|number|boolean|any[]|object;

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.initialize_();
  }

  getErrorState(): boolean {
    return this.hasAnError_;
  }

  // Event listener function for changing the type of input in the value cell of
  // this row depending on the value type of the selected policy.
  private changeInputType_(event: Event) {
    const selectElement: HTMLSelectElement = event.target! as HTMLSelectElement;
    const newValueType =
        selectElement.options[selectElement.selectedIndex]!.classList[0];
    const inputElement = this.getRequiredElement<HTMLInputElement>('.value');
    const inputElementCell = inputElement.parentNode! as HTMLElement;
    inputElement.remove();
    switch (newValueType) {
      case 'boolean':
        this.inputType_ = Boolean;
        const boolDropdown = document.createElement('select');
        boolDropdown.classList.add('value');
        // By default, have labels true/false for boolean policies, but use
        // enable/disable or allow/disallow if the policy name contains 'enable'
        // or 'allow' respectively.
        const boolOptions = {
          'true': ['true', 'enabled', 'allow'],
          'false': ['false', 'disabled', 'disallow'],
        };
        let boolOptionIndex = 0;
        const policyNameLower =
            selectElement.options[selectElement.selectedIndex]!.value
                .toLowerCase();
        if (policyNameLower.includes('enable')) {
          boolOptionIndex = 1;
        } else if (policyNameLower.includes('allow')) {
          boolOptionIndex = 2;
        }
        const optionsArray: string[] = [
          boolOptions['true'][boolOptionIndex]!,
          boolOptions['false'][boolOptionIndex]!,
        ];
        optionsArray.forEach((option: string) => {
          const optionElement = document.createElement('option');
          optionElement.textContent = option;
          optionElement.value = String(boolOptions['true'].includes(option));
          boolDropdown.appendChild(optionElement);
        });
        inputElementCell.appendChild(boolDropdown);
        break;
      case 'integer':
        this.inputType_ = Number;
        const numInput = document.createElement('input');
        numInput.type = 'number';
        numInput.classList.add('value');
        inputElementCell.appendChild(numInput);
        break;
      case 'string':
        this.inputType_ = String;
        const strInput = document.createElement('input');
        strInput.type = 'text';
        strInput.classList.add('value');
        inputElementCell.appendChild(strInput);
        break;
      case 'list':
        this.inputType_ = Array;
        const listInput = document.createElement('input');
        listInput.type = 'text';
        listInput.classList.add('value');
        inputElementCell.appendChild(listInput);
        break;
      case 'dictionary':
        this.inputType_ = Object;
        const dictInput = document.createElement('input');
        dictInput.type = 'text';
        dictInput.classList.add('value');
        inputElementCell.appendChild(dictInput);
        break;
      default:
        assertNotReached();
    }
  }

  // Function that initializes the policy selection dropdowns and delete
  // button for the current row.
  private initialize_() {
    const policyNameDropdown = this.getRequiredElement('.name');
    policyNameDropdown.addEventListener(
        'change', this.changeInputType_.bind(this));

    // Populate the policy name dropdown with all policy names.
    const policyNamesToTypes =
        JSON.parse(loadTimeData.getString('policyNamesToTypes'));
    for (const name in policyNamesToTypes) {
      const currOption = document.createElement('option');
      currOption.textContent = name;
      currOption.classList.add(policyNamesToTypes[name]);
      policyNameDropdown.appendChild(currOption);
    }

    // Add an event listener for this row's delete button.
    this.getRequiredElement('.remove-btn')
        .addEventListener('click', this.remove.bind(this));

    // Set the value attributes of the policy type dropdown options.
    const idToValue = [
      {id: 'scopeUser', value: PolicyScope.SCOPE_USER_VAL},
      {id: 'scopeDevice', value: PolicyScope.SCOPE_DEVICE_VAL},
      {id: 'levelRecommended', value: PolicyLevel.LEVEL_RECOMMENDED_VAL},
      {id: 'levelMandatory', value: PolicyLevel.LEVEL_MANDATORY_VAL},
      {
        id: 'sourceEnterpriseDefault',
        value: PolicySource.SOURCE_ENTERPRISE_DEFAULT,
      },
      {id: 'sourceCommandLine', value: PolicySource.SOURCE_COMMAND_LINE_VAL},
      {id: 'sourceCloud', value: PolicySource.SOURCE_CLOUD_VAL},
      {
        id: 'sourceActiveDirectory',
        value: PolicySource.SOURCE_ACTIVE_DIRECTORY_VAL,
      },
      {id: 'sourcePlatform', value: PolicySource.SOURCE_PLATFORM_VAL},
      {id: 'sourceMerged', value: PolicySource.SOURCE_MERGED_VAL},
      {id: 'sourceCloudFromAsh', value: PolicySource.SOURCE_CLOUD_FROM_ASH_VAL},
      {
        id: 'sourceRestrictedManagedGuestSessionOverride',
        value:
            PolicySource.SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL,
      },
    ];

    for (const pair of idToValue) {
      this.getRequiredElement(`#${pair.id}`)
          .setAttribute('value', String(pair.value));
    }
  }

  setInitialValues(initialValues: PolicyInfo) {
    const policyNameInput = this.getRequiredElement<HTMLInputElement>('.name');
    const policySourceInput =
        this.getRequiredElement<HTMLInputElement>('.source');
    const policyLevelInput =
        this.getRequiredElement<HTMLInputElement>('.level');
    const policyScopeInput =
        this.getRequiredElement<HTMLInputElement>('.target');
    const policyValueInput =
        this.getRequiredElement<HTMLInputElement>('.value');

    policyNameInput.value = initialValues.name;
    policySourceInput.value = String(initialValues.source);
    policyLevelInput.value = String(initialValues.level);
    policyScopeInput.value = String(initialValues.scope);
    policyValueInput.value = String(initialValues.value);
  }

  // Event listener function for setting the select element background back to
  // white after being highlighted in red, and then clicked by the user.
  private resetErrorState_(event: Event) {
    (event.target! as HTMLElement).classList.remove('error');
    this.errorEvents_.remove(event.target!);
    this.hasAnError_ = false;
  }
  // Helper method for highlighting an element in red and adding an event
  // listener to get rid of the element highlight on focus, for elements with
  // invalid input.
  private setInErrorState_(inputElement: HTMLElement) {
    inputElement.classList.add('error');
    this.errorEvents_.add(
        inputElement, 'focus', this.resetErrorState_.bind(this));
    this.hasAnError_ = true;
  }

  // Class method for returning the value for this policy (the value in the
  // value cell of this row).
  getPolicyValue(): string|number|boolean|any[]|object {
    const inputElement = this.getRequiredElement<HTMLInputElement>('.value');
    // If the policy expects a string, any input is valid.
    if (this.inputType_ === String) {
      return inputElement.value.toString();
    }
    try {
      const obj = JSON.parse(inputElement.value);
      if (obj !== undefined && obj.constructor === this.inputType_) {
        return obj;
      }
      throw new Error();
    } catch {
      this.setInErrorState_(inputElement);
    }
    return '';
  }

  // Class method for returning the name, level, source or scope set in this
  // row.
  getPolicyAttribute(attributeName: string): string {
    const inputElement: HTMLSelectElement =
        this.getRequiredElement<HTMLSelectElement>(`.${attributeName}`);
    if (inputElement.options[inputElement.selectedIndex]!.hidden) {
      this.setInErrorState_(inputElement);
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
