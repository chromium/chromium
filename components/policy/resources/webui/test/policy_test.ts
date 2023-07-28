// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './policy_test_table.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {PolicyTestTableElement} from './policy_test_table.js';

/**
 * Must be kept in sync with the C++ enums of the same names (see
 * components/policy/core/common/policy_types.h).
 */
export const enum PolicyScope {
  SCOPE_USER_VAL = 0,
  SCOPE_DEVICE_VAL = 1,
}

export const enum PolicyLevel {
  LEVEL_RECOMMENDED_VAL = 0,
  LEVEL_MANDATORY_VAL = 1,
}

export const enum PolicySource {
  SOURCE_ENTERPRISE_DEFAULT = 0,
  SOURCE_COMMAND_LINE_VAL = 1,
  SOURCE_CLOUD_VAL = 2,
  SOURCE_ACTIVE_DIRECTORY_VAL = 3,
  SOURCE_PLATFORM_VAL = 5,
  SOURCE_MERGED_VAL = 7,
  SOURCE_CLOUD_FROM_ASH_VAL = 8,
  SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL = 9,
}


// Simple object containing all policy information as a map between information
// type and value.
export interface PolicyInfo {
  name: string;
  source: PolicySource;
  scope: PolicyScope;
  level: PolicyLevel;
  value: string|number|boolean|any[]|object;
}

// Object mapping policy detail types to their values.
export interface PolicyInfoForExport {
  source: string;
  scope: string;
  level: string;
  value: any;
}

// Object mapping policy names to their corresponding PolicyDetails objects.
export interface PoliciesForExport {
  policyValues: {
    chrome: {
      name: 'Chrome Test Policies',
      policies: {[key: string]: PolicyInfoForExport},
    },
  };
}

function initialize() {
  getRequiredElement('import-policies-file-input')
      .addEventListener('change', uploadPoliciesFile);
  getRequiredElement('apply-policies').addEventListener('click', applyPolicies);
  getRequiredElement('revert-applied-policies')
      .addEventListener('click', resetPolicies);
  getRequiredElement('clear-policies').addEventListener('click', clearPolicies);
  getRequiredElement('export-policies-json')
      .addEventListener('click', exportAndDownloadPolicies);
}

function uploadPoliciesFile() {
  const fileInput =
      getRequiredElement<HTMLInputElement>('import-policies-file-input');
  // Get selected file
  const jsonFile = fileInput.files?.length == 1 ? fileInput.files![0] : null;
  if (jsonFile) {
    applyPoliciesFromFile(jsonFile!);
  }
}

function applyPoliciesFromFile(jsonFile: File) {
  // Read file as string
  const reader = new FileReader();
  reader.readAsText(jsonFile as Blob);

  reader.addEventListener(
      'load',
      () => {
        const fileInput =
            getRequiredElement<HTMLInputElement>('import-policies-file-input');

        // Extension policies are ignored, they are not supported on this page
        // TODO(b:293339258): Verify imported file is valid and display error
        // message if invalid
        const policies = JSON.parse(
            reader.result as string)['policyValues']['chrome']['policies'];

        const policyTable =
            getRequiredElement<PolicyTestTableElement>('policy-test-table');

        // Empty policy table
        policyTable.clearRows();

        // Add row for each policy
        for (const [key, value] of Object.entries(policies)) {
          if ((key as string)[0] == '_') {
            continue;
          }

          policyTable.addRow(convertToPolicyInfo(
              key as string, value as {[key: string]: any}));
        }

        // Reset files
        fileInput.value = '';
      },
      false,
  );
}

function convertToPolicyInfo(policyName: string, value: {[key: string]: any}) {
  const sources: {[key: string]: PolicySource} = {
    'sourceEnterpriseDefault': PolicySource.SOURCE_ENTERPRISE_DEFAULT,
    'commandLine': PolicySource.SOURCE_COMMAND_LINE_VAL,
    'cloud': PolicySource.SOURCE_CLOUD_VAL,
    'sourceActiveDirectory': PolicySource.SOURCE_ACTIVE_DIRECTORY_VAL,
    'platform': PolicySource.SOURCE_PLATFORM_VAL,
    'merged': PolicySource.SOURCE_MERGED_VAL,
    'cloud_from_ash': PolicySource.SOURCE_CLOUD_FROM_ASH_VAL,
    'restrictedManagedGuestSessionOverride':
        PolicySource.SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL,
  };

  const scopes: {[key: string]: PolicyScope} = {
    'user': PolicyScope.SCOPE_USER_VAL,
    'machine': PolicyScope.SCOPE_DEVICE_VAL,
  };

  const levels: {[key: string]: PolicyLevel} = {
    'recommended': PolicyLevel.LEVEL_RECOMMENDED_VAL,
    'mandatory': PolicyLevel.LEVEL_MANDATORY_VAL,
  };

  const policy: PolicyInfo = {
    name: policyName,
    source: sources[value['source']] ?? PolicySource.SOURCE_ENTERPRISE_DEFAULT,
    scope: scopes[value['scope']] ?? PolicyScope.SCOPE_USER_VAL,
    level: levels[value['level']] ?? PolicyLevel.LEVEL_MANDATORY_VAL,
    value: JSON.stringify(value['value']),
  };

  return policy;
}

function applyPolicies() {
  const jsonString =
      getRequiredElement<PolicyTestTableElement>('policy-test-table')
          .getTestPoliciesJsonString();
  if (jsonString) {
    // Disable the Apply policies button and re-enable after sending, to ensure
    // that the JSON string is not accidentally sent twice.
    getRequiredElement<HTMLButtonElement>('apply-policies').disabled = true;
    sendWithPromise('setLocalTestPolicies', jsonString);
    getRequiredElement<HTMLButtonElement>('revert-applied-policies').disabled =
        false;
    getRequiredElement<HTMLButtonElement>('apply-policies').disabled = false;
  }
}

function clearPolicies() {
  getRequiredElement<PolicyTestTableElement>('policy-test-table').clearRows();
  getRequiredElement<PolicyTestTableElement>('policy-test-table').addEmptyRow();
}

function resetPolicies(event: Event) {
  sendWithPromise('revertLocalTestPolicies');
  (event.target as HTMLButtonElement).disabled = true;
}

function exportAndDownloadPolicies() {
  const jsonString =
      getRequiredElement<PolicyTestTableElement>('policy-test-table')
          .getTestPoliciesJsonStringForExport();
  if (jsonString) {
    const blob = new Blob([jsonString], {type: 'application/json'});
    const blobUrl = URL.createObjectURL(blob);

    const link = document.createElement('a');
    link.href = blobUrl;
    link.download = 'test_policies.json';

    document.body.appendChild(link);

    link.dispatchEvent(new MouseEvent(
        'click', {bubbles: true, cancelable: true, view: window}));
  }
}

document.addEventListener('DOMContentLoaded', initialize);
