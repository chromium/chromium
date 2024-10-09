// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Page} from './policy_base.js';
import type {PolicyPrecedenceRowElement} from './policy_precedence_row.js';
import type {PolicyRowElement} from './policy_row.js';
import type {PolicyTableElement} from './policy_table.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';


// Have the main initialization function be called when the page finishes
// loading.
const page: Page = Page.getInstance();
document.addEventListener('DOMContentLoaded', () => {
  page.initialize();

  const promotionDismissButton = document.getElementById('promotion-dismiss-button');
  promotionDismissButton?.addEventListener('click' ,() => {
    chrome.send('setBannerDismissed');
    getRequiredElement('promotion-banner-section').hidden = true;
  });
});

// Functions for tests that directly inject JS to access certain UI elements.
function getPolicyFieldsets() {
  const statusBoxes = document.querySelectorAll('status-box');
  return Array.from(statusBoxes)
      .map(box => box.shadowRoot!.querySelector('.status-box-fields'));
}

function getAllPolicyTables() {
  return document.querySelectorAll('#policy-ui policy-table');
}

function getAllPolicyRows(policyTable: PolicyTableElement) {
  return policyTable.shadowRoot!.querySelectorAll('policy-row');
}

function getAllPolicyRowDivs(policyRow: PolicyRowElement) {
  const row = policyRow.shadowRoot!.querySelector('.policy.row');
  return row!.querySelectorAll('div');
}

function getPrecedenceRowValue() {
  const tables = document.querySelectorAll('policy-table');
  let precedenceRow = null;
  tables.forEach(table => {
    const row: PolicyPrecedenceRowElement|null =
        table.shadowRoot!.querySelector('policy-precedence-row');
    if (row) {
      precedenceRow = row.shadowRoot!.querySelector('.value');
    }
  });
  return precedenceRow;
}

function getRefreshIntervalEl() {
  return document.querySelector('status-box')!.shadowRoot!.querySelector(
      '.refresh-interval');
}

function getReportButtonVisibility() {
  const button: any = document.querySelector('button#upload-report');
  if (!button) {
    return 'none';
  }
  return button!.style.display.toString();
}

Object.assign(window, {
  getPolicyFieldsets,
  getAllPolicyTables,
  getAllPolicyRows,
  getAllPolicyRowDivs,
  getPrecedenceRowValue,
  getRefreshIntervalEl,
  getReportButtonVisibility,
});
