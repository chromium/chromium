// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {LevelNamesToValues, PolicyLevel, PolicyScope, PolicySource, Presets, ScopeNamesToValues, SourceNamesToValues} from './policy_test_browser_proxy.js';
import type {PolicyInfo, PolicySchema} from './policy_test_browser_proxy.js';
import {getCss} from './policy_test_row.css.js';
import {getHtml} from './policy_test_row.html.js';

export class PolicyTestRowElement extends CrLitElement {
  static get is() {
    return 'policy-test-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      namespaces: {type: Array},
      policyNamespace: {type: String},
      policyNames: {type: Array},
      policyName: {type: String},
      valueType: {type: String},
      policyValue: {type: String},
      policyPreset: {type: Number},
      policySource: {type: Number},
      policyScope: {type: Number},
      policyLevel: {type: Number},
      presetDisabled: {type: Boolean},
      nameError: {type: Boolean},
      valueError: {type: Boolean},
      namespaceError: {type: Boolean},
      schema: {type: Object},
      initialValues: {type: Object},
    };
  }

  accessor namespaces: string[] = [];
  accessor policyNamespace: string = 'chrome';
  accessor policyNames: string[] = [];
  accessor policyName: string = '';
  accessor valueType: string = 'string';
  accessor policyValue: string = '';
  accessor policyPreset: number = Presets.PRESET_CUSTOM;
  accessor policySource: number = PolicySource.SOURCE_ENTERPRISE_DEFAULT_VAL;
  accessor policyScope: number = PolicyScope.SCOPE_USER_VAL;
  accessor policyLevel: number = PolicyLevel.LEVEL_MANDATORY_VAL;
  accessor presetDisabled: boolean = false;
  accessor nameError: boolean = false;
  accessor valueError: boolean = false;
  accessor namespaceError: boolean = false;
  accessor schema: PolicySchema|null = null;
  accessor initialValues: PolicyInfo|null = null;

  private schema_?: PolicySchema;

  constructor() {
    super();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('schema')) {
      if (this.schema) {
        this.setSchema(this.schema);
      }
    }
    if (changedProperties.has('initialValues')) {
      if (this.initialValues) {
        this.setInitialValues(this.initialValues);
      }
    }
  }

  override firstUpdated() {
    // Lit property bindings (e.g. .value) on <select> elements can be applied
    // before <option> children are rendered, causing the browser to reject
    // the value. Force-set them here once options are guaranteed to be in DOM.
    const presetSelect =
        this.shadowRoot.querySelector<HTMLSelectElement>('.preset')!;
    presetSelect.value = String(this.policyPreset);
    const sourceSelect =
        this.shadowRoot.querySelector<HTMLSelectElement>('.source')!;
    sourceSelect.value = String(this.policySource);
    const scopeSelect =
        this.shadowRoot.querySelector<HTMLSelectElement>('.scope')!;
    scopeSelect.value = String(this.policyScope);
    const levelSelect =
        this.shadowRoot.querySelector<HTMLSelectElement>('.level')!;
    levelSelect.value = String(this.policyLevel);
    const nsSelect =
        this.shadowRoot.querySelector<HTMLSelectElement>('.namespace')!;
    nsSelect.value = this.policyNamespace;
  }

  getErrorState(): boolean {
    return this.nameError || this.valueError || this.namespaceError;
  }

  getBoolOptions(): string[] {
    const options = {
      'true': ['True', 'Enabled', 'Allow'],
      'false': ['False', 'Disabled', 'Disallow'],
    };
    let index = 0;
    const policyNameLower = this.policyName.toLowerCase();
    if (policyNameLower.includes('enable')) {
      index = 1;
    } else if (policyNameLower.includes('allow')) {
      index = 2;
    }
    return [options['true'][index]!, options['false'][index]!];
  }

  setSchema(schema: PolicySchema) {
    this.schema_ = schema;
    this.updatePolicyNamespaces();
    this.updatePolicyNames();
  }

  private updatePolicyNamespaces() {
    if (!this.schema_) {
      return;
    }
    this.namespaces = Object.keys(this.schema_).toSorted((a, b) => {
      if (a === 'chrome') {
        return -1;
      }
      if (b === 'chrome') {
        return 1;
      }
      return a < b ? -1 : (b < a ? 1 : 0);
    });
  }

  updatePolicyNames() {
    if (!this.schema_ || !(this.policyNamespace in this.schema_)) {
      return;
    }
    this.policyNames = Object.keys(this.schema_[this.policyNamespace]!);
    this.updateValueType();
  }

  private updateValueType() {
    if (!this.schema_) {
      return;
    }
    const ns = this.policyNamespace;
    if (this.isValidPolicyName(ns, this.policyName)) {
      this.valueType = this.schema_[ns]![this.policyName]!;
    } else {
      this.valueType = 'string';
    }
  }

  private isValidPolicyName(policyNamespace: string, policyName: string) {
    return !!(
        this.schema_ && policyNamespace in this.schema_ &&
        policyName in this.schema_[policyNamespace]!);
  }

  onNamespaceChange(e: Event) {
    this.policyNamespace = (e.target as HTMLSelectElement).value;
    this.policyName = '';
    this.policyValue = '';
    this.updatePolicyNames();
    this.namespaceError = false;
  }

  onNameChange(e: Event) {
    this.policyName = (e.target as HTMLInputElement).value;
    if (this.valueType === 'boolean') {
      this.policyValue = 'true';
    } else {
      this.policyValue = '';
    }
    this.updateValueType();
    this.nameError = false;
  }

  onValueChange(e: Event) {
    this.policyValue = (e.target as HTMLSelectElement).value;
    this.valueError = false;
  }

  onValueInput(e: Event) {
    this.policyValue = (e.target as HTMLInputElement).value;
    this.valueError = false;
  }

  onPresetChange(e: Event) {
    this.policyPreset = parseInt((e.target as HTMLSelectElement).value);
    switch (this.policyPreset) {
      case Presets.PRESET_CUSTOM:
        this.presetDisabled = false;
        break;
      case Presets.PRESET_CBCM:
        this.presetDisabled = true;
        this.policySource = PolicySource.SOURCE_CLOUD_VAL;
        this.policyScope = PolicyScope.SCOPE_DEVICE_VAL;
        this.policyLevel = PolicyLevel.LEVEL_MANDATORY_VAL;
        break;
      case Presets.PRESET_LOCAL_MACHINE:
        this.presetDisabled = true;
        this.policySource = PolicySource.SOURCE_PLATFORM_VAL;
        this.policyScope = PolicyScope.SCOPE_DEVICE_VAL;
        this.policyLevel = PolicyLevel.LEVEL_MANDATORY_VAL;
        break;
      case Presets.PRESET_CLOUD_ACCOUNT:
        this.presetDisabled = true;
        this.policySource = PolicySource.SOURCE_CLOUD_VAL;
        this.policyScope = PolicyScope.SCOPE_USER_VAL;
        this.policyLevel = PolicyLevel.LEVEL_MANDATORY_VAL;
        break;
      default:
        break;
    }
    this.nameError = false;
    this.valueError = false;
    this.namespaceError = false;
  }

  protected onFocus(e: Event) {
    const target = e.target as HTMLElement;
    if (target.classList.contains('namespace')) {
      this.namespaceError = false;
    } else if (target.classList.contains('name')) {
      this.nameError = false;
    } else if (target.classList.contains('value')) {
      this.valueError = false;
    }
  }

  onSourceChange(e: Event) {
    this.policySource = parseInt((e.target as HTMLSelectElement).value);
  }

  onScopeChange(e: Event) {
    this.policyScope = parseInt((e.target as HTMLSelectElement).value);
  }

  onLevelChange(e: Event) {
    this.policyLevel = parseInt((e.target as HTMLSelectElement).value);
  }

  onRemoveClick() {
    this.fire('remove-row');
  }

  setInitialValues(initialValues: PolicyInfo) {
    this.policyNamespace = initialValues.namespace;
    this.updatePolicyNames();

    this.policySource = initialValues.source;
    this.policyLevel = initialValues.level;
    this.policyScope = initialValues.scope;
    this.policyName = initialValues.name;
    this.updateValueType();

    if (this.valueType === 'string') {
      this.policyValue = initialValues.value as string;
    } else {
      this.policyValue = JSON.stringify(initialValues.value);
    }
  }

  getPolicyValue(): string|number|boolean|unknown[]|object {
    if (this.valueType === 'string') {
      return this.policyValue;
    }
    try {
      const obj = JSON.parse(this.policyValue);
      let expectedType: StringConstructor|BooleanConstructor|NumberConstructor|
          ArrayConstructor|ObjectConstructor = String;
      switch (this.valueType) {
        case 'boolean':
          expectedType = Boolean;
          break;
        case 'integer':
        case 'number':
          expectedType = Number;
          break;
        case 'list':
          expectedType = Array;
          break;
        case 'dictionary':
          expectedType = Object;
          break;
        default:
          break;
      }
      if (obj !== undefined && obj.constructor === expectedType) {
        return obj;
      }
      throw new Error();
    } catch {
      this.valueError = true;
    }
    return '';
  }

  getPolicyNamespace(): string {
    if (this.schema_ && this.policyNamespace in this.schema_) {
      return this.policyNamespace;
    } else {
      this.namespaceError = true;
      return '';
    }
  }

  getPolicyName(): string {
    if (this.isValidPolicyName(this.policyNamespace, this.policyName)) {
      return this.policyName;
    } else {
      this.nameError = true;
      return '';
    }
  }

  getPolicyAttribute(attributeName: string): string {
    switch (attributeName) {
      case 'source':
        return String(this.policySource);
      case 'scope':
        return String(this.policyScope);
      case 'level':
        return String(this.policyLevel);
      default:
        assertNotReached();
    }
  }

  getStringPolicyAttribute(attributeName: string): string|undefined {
    const intVal = parseInt(this.getPolicyAttribute(attributeName));
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

declare global {
  interface HTMLElementTagNameMap {
    'policy-test-row': PolicyTestRowElement;
  }
}
customElements.define(PolicyTestRowElement.is, PolicyTestRowElement);
