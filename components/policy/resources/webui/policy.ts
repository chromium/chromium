// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './policy_app.js';

import type {PolicyPrecedenceRowElement} from './policy_precedence_row.js';
import type {PolicyRowElement} from './policy_row.js';
import type {PolicyTableElement} from './policy_table.js';

// Functions for tests that directly inject JS to access certain UI elements.
function getPolicyFieldsets() {
  const app = document.querySelector('policy-app');
  if (!app || !app.shadowRoot) {
    return [];
  }
  const statusBoxes = app.shadowRoot.querySelectorAll('status-box');
  return Array.from(statusBoxes)
      .map(
          box => box.shadowRoot ?
              box.shadowRoot.querySelector('.status-box-fields') :
              null)
      .filter((el): el is Element => !!el);
}

function getAllPolicyTables() {
  const app = document.querySelector('policy-app');
  if (!app || !app.shadowRoot) {
    return [];
  }
  return app.shadowRoot.querySelectorAll('#policy-ui policy-table');
}

function getAllPolicyRows(policyTable: PolicyTableElement) {
  if (!policyTable.shadowRoot) {
    return [];
  }
  return policyTable.shadowRoot.querySelectorAll('policy-row');
}

function getAllPolicyRowDivs(policyRow: PolicyRowElement) {
  const row = policyRow.shadowRoot ?
      policyRow.shadowRoot.querySelector('.policy.row') :
      null;
  return row ? row.querySelectorAll('div') : [];
}

function getPrecedenceRowValue() {
  const app = document.querySelector('policy-app');
  if (!app || !app.shadowRoot) {
    return null;
  }
  const tables = app.shadowRoot.querySelectorAll('policy-table');
  let precedenceRow: Element|null = null;
  tables.forEach(table => {
    const row: PolicyPrecedenceRowElement|null =
        table.shadowRoot.querySelector('policy-precedence-row');
    if (row) {
      precedenceRow = row.shadowRoot.querySelector('.value');
    }
  });
  return precedenceRow;
}

function getRefreshIntervalEl() {
  const app = document.querySelector('policy-app');
  if (!app || !app.shadowRoot) {
    return null;
  }
  const statusBox = app.shadowRoot.querySelector('status-box');
  if (!statusBox || !statusBox.shadowRoot) {
    return null;
  }
  return statusBox.shadowRoot.querySelector('.refresh-interval');
}

function getReportButtonVisibility() {
  const app = document.querySelector('policy-app');
  if (!app || !app.shadowRoot) {
    return 'none';
  }
  const button =
      app.shadowRoot.querySelector<HTMLElement>('button#upload-report');
  if (!button || button.hidden) {
    return 'none';
  }
  return 'block';
}

function reloadPolicies(): Promise<void> {
  const app = document.querySelector('policy-app');
  const reloadPoliciesBtn = app && app.shadowRoot ?
      app.shadowRoot.querySelector<HTMLElement&{disabled: boolean}>(
          '#reload-policies') :
      null;
  if (!reloadPoliciesBtn) {
    return Promise.reject(new Error('Reload button not found'));
  }
  reloadPoliciesBtn.click();
  return new Promise<void>(resolve => {
    const waitForPoliciesToReload = () => {
      if (reloadPoliciesBtn.disabled) {
        window.requestIdleCallback(waitForPoliciesToReload);
      } else {
        resolve();
      }
    };
    window.requestIdleCallback(waitForPoliciesToReload);
  });
}

Object.assign(window, {
  getPolicyFieldsets,
  getAllPolicyTables,
  getAllPolicyRows,
  getAllPolicyRowDivs,
  getPrecedenceRowValue,
  getRefreshIntervalEl,
  getReportButtonVisibility,
  reloadPolicies,
});
