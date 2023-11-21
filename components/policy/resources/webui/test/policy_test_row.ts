// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {LevelNamesToValues, PolicyInfo, PolicyLevel, PolicyScope, PolicySource, PresetAtrributes, Presets, ScopeNamesToValues, SourceNamesToValues} from './policy_test_browser_proxy.js';
import {getTemplate} from './policy_test_row.html.js';

export class PolicyTestRowElement extends CustomElement {
  private hasAnError_: boolean = false;
  private errorEvents_: EventTracker = new EventTracker();
  private inputType_: string|number|boolean|any[]|object;
  private policyNamesToTypes_: {[key: string]: string};

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

  // Event listener function for changing the input type when a policy name is
  // selected.
  private changeInputTypeEvent_(event: Event) {
    this.changeInputType_(event.target! as HTMLInputElement);
  }

  private changeInputType_(nameInput: HTMLInputElement) {
    // Return if invalid policy name
    if (!this.isValidPolicyName_(nameInput.value)) {
      this.setInErrorState_(nameInput);
      return;
    }
    const newValueType = this.policyNamesToTypes_[nameInput.value];
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
          'true': ['True', 'Enabled', 'Allow'],
          'false': ['False', 'Disabled', 'Disallow'],
        };
        let boolOptionIndex = 0;
        const policyNameLower = nameInput.value.toLowerCase();
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
        const intInput = document.createElement('input');
        intInput.type = 'number';
        intInput.classList.add('value');
        inputElementCell.appendChild(intInput);
        break;
      case 'number':
        this.inputType_ = Number;
        const numInput = document.createElement('input');
        numInput.type = 'number';
        numInput.step = 'any';
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

  // Helper method for disabling/enabling the source, scope and level dropdowns
  // if disabled is true/false and setting their values to those given in
  // presetAttributes.
  private changeAttributesFromPreset_(
      disabled: boolean, presetAttributes?: PresetAtrributes) {
    ['.source', '.scope', '.level'].forEach((attributeSelector: string) => {
      const attributeDropdown =
          this.getRequiredElement<HTMLSelectElement>(attributeSelector);
      attributeDropdown.disabled = disabled;
      if (presetAttributes) {
        attributeDropdown.value = String(Object.values(
            presetAttributes)[Object.keys(presetAttributes)
                                  .indexOf(attributeSelector.substring(1))]);
      }
    });
  }

  // Event listener method for changing the selected values in the source, scope
  // and level dropdowns when the user selects a preset.
  private changePreset_(event: Event) {
    const selectElement = event.target! as HTMLSelectElement;
    const presetToApply = parseInt(selectElement.value);
    switch (presetToApply) {
      case Presets.PRESET_CUSTOM:
        this.changeAttributesFromPreset_(false);
        break;
      case Presets.PRESET_CBCM:
        this.changeAttributesFromPreset_(true, {
          source: PolicySource.SOURCE_CLOUD_VAL,
          scope: PolicyScope.SCOPE_DEVICE_VAL,
          level: PolicyLevel.LEVEL_MANDATORY_VAL,
        });
        break;
      case Presets.PRESET_LOCAL_MACHINE:
        this.changeAttributesFromPreset_(true, {
          source: PolicySource.SOURCE_PLATFORM_VAL,
          scope: PolicyScope.SCOPE_DEVICE_VAL,
          level: PolicyLevel.LEVEL_MANDATORY_VAL,
        });
        break;
      case Presets.PRESET_CLOUD_ACCOUNT:
        this.changeAttributesFromPreset_(true, {
          source: PolicySource.SOURCE_CLOUD_VAL,
          scope: PolicyScope.SCOPE_USER_VAL,
          level: PolicyLevel.LEVEL_MANDATORY_VAL,
        });
        break;
      default:
        assertNotReached();
    }
  }

  // Function that verifies policy name is a valid.
  private isValidPolicyName_(policyName: string) {
    if (policyName in this.policyNamesToTypes_) {
      return true;
    } else {
      return false;
    }
  }

  // Function that initializes the policy selection dropdowns and delete
  // button for the current row.
  private initialize_() {
    const policyNameDatalist = this.getRequiredElement('#policy-name-list');
    const policyNameInput = this.getRequiredElement('.name');
    policyNameInput.addEventListener(
        'change', this.changeInputTypeEvent_.bind(this));

    // Populate the policy name dropdown with all policy names.
    this.policyNamesToTypes_ =
        JSON.parse(loadTimeData.getString('policyNamesToTypes'));
    for (const name in this.policyNamesToTypes_) {
      const currOption = document.createElement('option');
      currOption.textContent = name;
      policyNameDatalist.appendChild(currOption);
    }

    // Add an event listener for this row's delete button.
    this.getRequiredElement('.remove-btn')
        .addEventListener('click', this.remove.bind(this));

    // Add event listeners for this row's preset select options.
    const policyPresetDropdown =
        this.getRequiredElement<HTMLSelectElement>('.preset');
    policyPresetDropdown.addEventListener(
        'change', this.changePreset_.bind(this));

    // Set the value attributes of the policy type and preset dropdown options.
    const idToValue = [
      {id: 'scopeUser', value: PolicyScope.SCOPE_USER_VAL},
      {id: 'scopeDevice', value: PolicyScope.SCOPE_DEVICE_VAL},
      {id: 'levelRecommended', value: PolicyLevel.LEVEL_RECOMMENDED_VAL},
      {id: 'levelMandatory', value: PolicyLevel.LEVEL_MANDATORY_VAL},
      {
        id: 'sourceEnterpriseDefault',
        value: PolicySource.SOURCE_ENTERPRISE_DEFAULT_VAL,
      },
      {id: 'sourceCommandLine', value: PolicySource.SOURCE_COMMAND_LINE_VAL},
      {id: 'sourceCloud', value: PolicySource.SOURCE_CLOUD_VAL},
      // <if expr="is_chromeos">
      {
        id: 'sourceActiveDirectory',
        value: PolicySource.SOURCE_ACTIVE_DIRECTORY_VAL,
      },
      // </if>
      {id: 'sourcePlatform', value: PolicySource.SOURCE_PLATFORM_VAL},
      {id: 'sourceMerged', value: PolicySource.SOURCE_MERGED_VAL},
      // <if expr="is_chromeos">
      {id: 'sourceCloudFromAsh', value: PolicySource.SOURCE_CLOUD_FROM_ASH_VAL},
      {
        id: 'sourceRestrictedManagedGuestSessionOverride',
        value:
            PolicySource.SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL,
      },
      // </if>
      {id: 'custom', value: Presets.PRESET_CUSTOM},
      {id: 'cbcm', value: Presets.PRESET_CBCM},
      {id: 'localMachine', value: Presets.PRESET_LOCAL_MACHINE},
      {id: 'cloudAccount', value: Presets.PRESET_CLOUD_ACCOUNT},
    ];

    for (const pair of idToValue) {
      this.getRequiredElement(`#${pair.id}`)
          .setAttribute('value', String(pair.value));
    }
  }

  // Class method for setting the name, source, scope, level and value cells for
  // this row.
  setInitialValues(initialValues: PolicyInfo) {
    const policyNameInput = this.getRequiredElement<HTMLInputElement>('.name');
    const policySourceInput =
        this.getRequiredElement<HTMLInputElement>('.source');
    const policyLevelInput =
        this.getRequiredElement<HTMLInputElement>('.level');
    const policyScopeInput =
        this.getRequiredElement<HTMLInputElement>('.scope');

    policySourceInput.value = String(initialValues.source);
    policyLevelInput.value = String(initialValues.level);
    policyScopeInput.value = String(initialValues.scope);

    // Change input type according to policy, set value to new input
    policyNameInput.value = initialValues.name;
    this.changeInputType_(policyNameInput);

    const policyValueInput =
        this.getRequiredElement<HTMLInputElement>('.value');
    if (this.inputType_ === String) {
      initialValues.value = this.trimSurroundingQuotes_(initialValues.value);
    }
    policyValueInput.value = JSON.stringify(initialValues.value);
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

  // Helper method for trimming the surrounding double quotes in the string, if
  // any are present.
  private trimSurroundingQuotes_(stringToTrim: string): string {
    if (stringToTrim.length < 2) {
      return stringToTrim;
    }
    stringToTrim.trim();
    if (stringToTrim.charAt(0) === '"' &&
        stringToTrim.charAt(stringToTrim.length - 1) === '"') {
      stringToTrim = stringToTrim.substring(1, stringToTrim.length - 1);
    }
    stringToTrim.trim();
    return stringToTrim;
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

  // Class method for returning the name for this policy (the value in the
  // name cell of this row)
  getPolicyName(): string {
    const inputElement: HTMLInputElement =
        this.getRequiredElement<HTMLInputElement>('.name');

    if (this.isValidPolicyName_(inputElement.value)) {
      return inputElement.value;
    } else {
      this.setInErrorState_(inputElement);
      return '';
    }
  }

  // Class method for returning the level, source or scope set in this
  // row.
  getPolicyAttribute(attributeName: string): string {
    return this.getRequiredElement<HTMLSelectElement>(`.${attributeName}`)
        .value;
  }

  // Class method for returning the string value of the given attribute in this
  // row. Should only be used for enum attributes (level, scope and source).
  getStringPolicyAttribute(attributeName: string): string|undefined {
    const intVal: number =
        parseInt(String(this.getPolicyAttribute(`${attributeName}`)));
    switch (attributeName) {
      case 'level':
        return Object.keys(LevelNamesToValues)
            .find(name => LevelNamesToValues[name] === intVal);
      case 'scope':
        return Object.keys(ScopeNamesToValues)
            .find(name => ScopeNamesToValues[name] === intVal);
      case 'source':
        return Object.keys(SourceNamesToValues)
            .find(name => SourceNamesToValues[name] === intVal);
      default:
        assertNotReached();
    }
  }
}

// Declare the custom element.
declare global {
  interface HTMLElementTagNameMap {
    'policy-test-row': PolicyTestRowElement;
  }
}
customElements.define('policy-test-row', PolicyTestRowElement);
