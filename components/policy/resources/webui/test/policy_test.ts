// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './policy_test_table.js';

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {PolicyTestTableElement} from './policy_test_table.js';

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

        const policies = JSON.parse(reader.result as string) as object[];
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
