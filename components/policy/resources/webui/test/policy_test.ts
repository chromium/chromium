// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolicyTestTableElement} from './policy_test_table';

function initialize() {
  document.querySelector('#import-policies-file-input')!.addEventListener(
      'change', uploadPoliciesFile);
}

function uploadPoliciesFile() {
  const fileInput = document.getElementById('import-policies-file-input')! as
      HTMLInputElement;
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
            document.getElementById('import-policies-file-input')! as
            HTMLInputElement;

        const policies = JSON.parse(reader.result as string) as object[];
        const policyTable = document.getElementById('policy-test-table')! as
            PolicyTestTableElement;

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

document.addEventListener('DOMContentLoaded', initialize);
