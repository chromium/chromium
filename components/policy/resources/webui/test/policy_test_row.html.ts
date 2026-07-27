// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PolicyLevel, PolicyScope, PolicySource, Presets} from './policy_test_browser_proxy.js';
import type {PolicyTestRowElement} from './policy_test_row.js';

export function getHtml(this: PolicyTestRowElement) {
  return html`<!--_html_template_start_-->
    <div role="cell" class="name-cell" id="namespace-cell-heading">
      <label>$i18n{testTableNamespace}</label>
      <select class="namespace ${this.namespaceError ? 'error' : ''}"
          .value="${this.policyNamespace}"
          @change="${this.onNamespaceChange}"
          @focus="${this.onFocus}">
        ${this.namespaces.map(ns => html`
          <option value="${ns}">${ns === 'chrome' ? 'Chrome' : ns}</option>
        `)}
      </select>
    </div>
    <div role="cell" class="name-cell">
      <label>$i18n{testTableName}</label>
      <input class="name ${this.nameError ? 'error' : ''}"
          list="policy-name-list" autocomplete="off"
          .value="${this.policyName}"
          placeholder="$i18n{testNameSelect}"
          @change="${this.onNameChange}"
          @focus="${this.onFocus}">
      <datalist id="policy-name-list">
        ${this.policyNames.map(
            name => html`<option value="${name}">${name}</option>`)}
      </datalist>
    </div>
    <div role="cell">
      <label>$i18n{testTableValue}</label>
      ${
      this.valueType === 'boolean' ?
          html`
        <select class="value ${this.valueError ? 'error' : ''}"
            .value="${this.policyValue}"
            @change="${this.onValueChange}"
            @focus="${this.onFocus}">
          <option value="true">${this.getBoolOptions()[0]}</option>
          <option value="false">${this.getBoolOptions()[1]}</option>
        </select>
      ` :
          this.valueType === 'integer' ?
          html`
        <input type="number" class="value ${this.valueError ? 'error' : ''}"
            .value="${this.policyValue}"
            @input="${this.onValueInput}"
            @focus="${this.onFocus}">
      ` :
          this.valueType === 'number' ?
          html`
        <input type="number" step="any" class="value ${this.valueError ? 'error' : ''}"
            .value="${this.policyValue}"
            @input="${this.onValueInput}"
            @focus="${this.onFocus}">
      ` :
          html`
        <input type="text" class="value ${this.valueError ? 'error' : ''}"
            .value="${this.policyValue}"
            @input="${this.onValueInput}"
            @focus="${this.onFocus}">
      `}
    </div>
    <div role="cell">
      <label>$i18n{testTablePreset}</label>
      <select class="preset" .value="${String(this.policyPreset)}" @change="${
      this.onPresetChange}">
        <option id="custom" value="${
      Presets.PRESET_CUSTOM}">$i18n{testTablePresetCustom}</option>
        <option id="cbcm" value="${Presets.PRESET_CBCM}">CBCM</option>
        <option id="localMachine" value="${
      Presets.PRESET_LOCAL_MACHINE}">$i18n{testTablePresetLocalMachine}</option>
        <option id="cloudAccount" value="${
      Presets.PRESET_CLOUD_ACCOUNT}">$i18n{testTablePresetCloudAccount}</option>
      </select>
    </div>
    <div role="cell">
      <label>$i18n{testTableSource}</label>
      <select class="source" ?disabled="${this.presetDisabled}" .value="${
      String(this.policySource)}" @change="${this.onSourceChange}">
        <option id="sourceEnterpriseDefault"
            value="${PolicySource.SOURCE_ENTERPRISE_DEFAULT_VAL}"
            title="POLICY_SOURCE_ENTERPRISE_DEFAULT">
          $i18n{sourceEnterpriseDefault}
        </option>
        <option id="sourceCommandLine"
            value="${PolicySource.SOURCE_COMMAND_LINE_VAL}"
            title="POLICY_SOURCE_COMMAND_LINE">
          $i18n{sourceCommandLine}
        </option>
        <option id="sourceCloud" value="${PolicySource.SOURCE_CLOUD_VAL}"
        title="POLICY_SOURCE_CLOUD">
          $i18n{sourceCloud}
        </option>
        <if expr="is_chromeos">
        <option id="sourceActiveDirectory"
            value="${PolicySource.SOURCE_ACTIVE_DIRECTORY_VAL}"
            title="POLICY_SOURCE_ACTIVE_DIRECTORY">
          $i18n{sourceActiveDirectory}
        </option>
        </if>
        <option id="sourcePlatform" value="${PolicySource.SOURCE_PLATFORM_VAL}"
            title="POLICY_SOURCE_PLATFORM">
          $i18n{sourcePlatform}
        </option>
        <option id="sourceMerged" value="${PolicySource.SOURCE_MERGED_VAL}"
            title="POLICY_SOURCE_MERGED">
          $i18n{sourceMerged}
        </option>
        <if expr="is_chromeos">
        <option id="sourceCloudFromAsh"
            value="${PolicySource.SOURCE_CLOUD_FROM_ASH_VAL}"
            title="POLICY_SOURCE_CLOUD_FROM_ASH">
          $i18n{sourceCloudFromAsh}
        </option>
        <option id="sourceRestrictedManagedGuestSessionOverride"
            value="${
              PolicySource.SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL}"
            title="POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE">
          $i18n{sourceRestrictedManagedGuestSessionOverride}
        </option>
        </if>
      </select>
    </div>
    <div role="cell">
      <label>$i18n{testTableScope}</label>
      <select class="scope" ?disabled="${this.presetDisabled}" .value="${
      String(this.policyScope)}" @change="${this.onScopeChange}">
        <option id="scopeUser"
            value="${PolicyScope.SCOPE_USER_VAL}" title="POLICY_SCOPE_USER">
          $i18n{scopeUser}
        </option>
        <option id="scopeDevice"
            value="${PolicyScope.SCOPE_DEVICE_VAL}" title="POLICY_SCOPE_DEVICE">
          $i18n{scopeDevice}
        </option>
      </select>
    </div>
    <div role="cell">
      <label>$i18n{testTableLevel}</label>
      <select class="level" ?disabled="${this.presetDisabled}" .value="${
      String(this.policyLevel)}" @change="${this.onLevelChange}">
        <option id="levelMandatory"
            value="${PolicyLevel.LEVEL_MANDATORY_VAL}"
            title="POLICY_LEVEL_MANDATORY">
          $i18n{levelMandatory}
        </option>
        <option id="levelRecommended"
            value="${PolicyLevel.LEVEL_RECOMMENDED_VAL}"
            title="POLICY_LEVEL_RECOMMENDED">
          $i18n{levelRecommended}
        </option>
      </select>
    </div>
    <div role="cell" class="row-remove-btn-cell">
      <button class="remove-btn" @click="${this.onRemoveClick}">–</button>
    </div>
  <!--_html_template_end_-->`;
}
