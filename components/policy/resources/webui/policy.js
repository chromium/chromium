// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Page} from './policy_base.js';

// Have the main initialization function be called when the page finishes
// loading.
const page = Page.getInstance();
document.addEventListener('DOMContentLoaded', () => {
  page.initialize();
});

// Functions for tests that directly inject JS to access certain UI elements.
function getPolicyFieldsets() {
  const statusSection = document.querySelector('#status-section');
  return statusSection.querySelectorAll('fieldset');
}

function getAllPolicyTables() {
  return document.querySelector('#policy-ui').querySelectorAll('.policy-table');
}

function getAllPolicyRows(policyTable) {
  return policyTable.querySelectorAll('.policy.row');
}

function getAllPolicyRowDivs(policyRow) {
  return policyRow.querySelectorAll('div');
}

function getPrecedenceRowValue() {
  const precedenceRow =
      document.querySelector('#policy-ui')
          .querySelector('.policy-table .precedence.row > .value');
  return precedenceRow;
}

function getRefreshIntervalEl() {
  return document.querySelector('#status-box-container .refresh-interval');
}

Object.assign(window, {
  getPolicyFieldsets,
  getAllPolicyTables,
  getAllPolicyRows,
  getAllPolicyRowDivs,
  getPrecedenceRowValue,
  getRefreshIntervalEl,
});
