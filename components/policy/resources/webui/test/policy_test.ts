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
export enum PolicyScope {
  SCOPE_USER_VAL = 0,
  SCOPE_DEVICE_VAL = 1,
}

export enum PolicyLevel {
  LEVEL_RECOMMENDED_VAL = 0,
  LEVEL_MANDATORY_VAL = 1,
}

export enum PolicySource {
  SOURCE_ENTERPRISE_DEFAULT = 0,
  SOURCE_COMMAND_LINE_VAL = 1,
  SOURCE_CLOUD_VAL = 2,
  SOURCE_ACTIVE_DIRECTORY_VAL = 3,
  SOURCE_PLATFORM_VAL = 5,
  SOURCE_MERGED_VAL = 7,
  SOURCE_CLOUD_FROM_ASH_VAL = 8,
  SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE_VAL = 9,
}

export interface PolicyInfo {
  name: string;
  source: PolicySource;
  scope: PolicyScope;
  level: PolicyLevel;
  value: string;
}

function initialize() {
  getRequiredElement('import-policies-file-input')
      .addEventListener('change', uploadPoliciesFile);
  getRequiredElement('apply-policies').addEventListener('click', applyPolicies);
  getRequiredElement('revert-applied-policies')
      .addEventListener('click', resetPolicies);
  getRequiredElement('clear-policies').addEventListener('click', clearPolicies);
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

        const policies: PolicyInfo[] =
            JSON.parse(reader.result as string) as PolicyInfo[];
        const policyTable =
            getRequiredElement<PolicyTestTableElement>('policy-test-table');

        // Empty policy table
        policyTable.clearRows();

        // Add row for each policy
        for (let i = 0; i < policies.length; i++) {
          policyTable.addRow(policies[i]!);
        }

        // Reset files
        fileInput.value = '';
      },
      false,
  );
}

function applyPolicies() {
  const jsonString =
      getRequiredElement<PolicyTestTableElement>('policy-test-table')
          .getTestPoliciesAsJsonString();
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
}

function resetPolicies(event: Event) {
  sendWithPromise('revertLocalTestPolicies');
  (event.target as HTMLButtonElement).disabled = true;
}

document.addEventListener('DOMContentLoaded', initialize);
