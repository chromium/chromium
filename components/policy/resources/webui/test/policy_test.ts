// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './policy_test_table.js';

import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {LevelNamesToValues, PolicyInfo, PolicyLevel, PolicyScope, PolicySource, PolicyTestBrowserProxy, ScopeNamesToValues, SourceNamesToValues} from './policy_test_browser_proxy.js';
import {PolicyTestTableElement} from './policy_test_table.js';

const policyTestBrowserProxy: PolicyTestBrowserProxy =
    PolicyTestBrowserProxy.getInstance();

function initialize() {
  getRequiredElement('import-policies-file-input')
      .addEventListener('change', uploadPoliciesFile);
  getRequiredElement('apply-policies').addEventListener('click', applyPolicies);
  getRequiredElement('revert-applied-policies')
      .addEventListener('click', resetPolicies);
  getRequiredElement('clear-policies').addEventListener('click', clearPolicies);
  getRequiredElement('export-policies-json')
      .addEventListener('click', exportAndDownloadPolicies);
  getRequiredElement('restart-browser')
      .addEventListener('click', restartBrowser);
  getRequiredElement<HTMLTextAreaElement>('profile-separation-response')
      .placeholder = `Fake profile separation external response:
  {
    "policyValue": "ManagedAccountsSigninRestrictions value",
    "profileSeparationSettings": 1,
    "profileSeparationDataMigrationSettings": 2
  }`;
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
        try {
          const policyTable =
              getRequiredElement<PolicyTestTableElement>('policy-test-table');
          // Empty policy table
          policyTable.clearRows();

          // Populate the test table after determining whether the array or
          // object format is used.
          const policies = JSON.parse(reader.result as string);
          if (policies.constructor === Array) {
            // Add row for each policy.
            policies.forEach((policy: PolicyInfo) => {
              policyTable.addRow(policy);
            });
          } else {
            const policiesObj = policies['policyValues']['chrome']['policies'];

            // Add row for each policy
            for (const [key, value] of Object.entries(policiesObj)) {
              if (key.startsWith('_')) {
                continue;
              }

              policyTable.addRow(
                  convertToPolicyInfo(key, value as {[key: string]: any}));
            }
          }

          // Reset files
          fileInput.value = '';
        } catch {
          alert('Invalid file format.');
        }

      },
      false,
  );
}

function convertToPolicyInfo(policyName: string, value: {[key: string]: any}) {
  const policy: PolicyInfo = {
    name: policyName,
    source: Number(SourceNamesToValues[value['source']]) ??
        PolicySource.SOURCE_ENTERPRISE_DEFAULT_VAL,
    scope: Number(ScopeNamesToValues[value['scope']]) ??
        PolicyScope.SCOPE_USER_VAL,
    level: Number(LevelNamesToValues[value['level']]) ??
        PolicyLevel.LEVEL_MANDATORY_VAL,
    value: JSON.stringify(value['value']),
  };

  return policy;
}

async function applyPolicies() {
  const policies =
      getRequiredElement<PolicyTestTableElement>('policy-test-table')
          .getTestPoliciesJsonString();
  const profileSeparationResponse =
      getRequiredElement<HTMLTextAreaElement>('profile-separation-response')
          .value;

  // If no policy is set, there is nothing to do.
  if (!policies && !profileSeparationResponse) {
    return;
  }

  // Disable the Apply policies button and re-enable after sending, to ensure
  // that the JSON string is not accidentally sent twice.
  getRequiredElement<HTMLButtonElement>('apply-policies').disabled = true;
  const userAffiliation =
      getRequiredElement<HTMLInputElement>('user-affiliated').checked;
  await policyTestBrowserProxy.setUserAffiliation(userAffiliation);
  await policyTestBrowserProxy.applyTestPolicies(
      policies || '[]', profileSeparationResponse);

  getRequiredElement<HTMLButtonElement>('revert-applied-policies').disabled =
      false;
  getRequiredElement<HTMLButtonElement>('apply-policies').disabled = false;
}

function clearPolicies() {
  getRequiredElement<PolicyTestTableElement>('policy-test-table').clearRows();
  getRequiredElement<HTMLTextAreaElement>('profile-separation-response').value =
      '';
  getRequiredElement<PolicyTestTableElement>('policy-test-table').addEmptyRow();
}

function resetPolicies(event: Event) {
  policyTestBrowserProxy.revertTestPolicies();
  (event.target as HTMLButtonElement).disabled = true;
}

function exportAndDownloadPolicies() {
  const jsonString =
      getRequiredElement<PolicyTestTableElement>('policy-test-table')
          .getTestPoliciesJsonString();
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

function restartBrowser() {
  const jsonString =
      getRequiredElement<PolicyTestTableElement>('policy-test-table')
          .getTestPoliciesJsonString();
  if (jsonString) {
    policyTestBrowserProxy.restartWithTestPolicies(jsonString);
  }
}

document.addEventListener('DOMContentLoaded', initialize);
