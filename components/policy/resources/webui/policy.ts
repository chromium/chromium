// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './policy_app.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

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
  return app.shadowRoot.querySelectorAll<PolicyTableElement>(
      '#policy-ui policy-table');
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

function getChromePolicyTable(): PolicyTableElement|undefined {
  return Array.from(getAllPolicyTables())
      .find(t => t.dataModel?.id === 'chrome');
}

function getChromePolicyRow(name: string): PolicyRowElement|undefined {
  const table = getChromePolicyTable();
  return table ?
      Array.from(getAllPolicyRows(table)).find(r => r.policy?.name === name) :
      undefined;
}

function visibleClasses(root: ShadowRoot, selector: string): string[] {
  const elements = root.querySelectorAll(`${selector}:not([hidden])`);
  return Array.from(elements, el => el.classList[0]!);
}

function getVisiblePolicies(): string[] {
  const table = getChromePolicyTable();
  if (!table) {
    return [];
  }
  return Array.from(getAllPolicyRows(table))
      .filter(r => !r.hidden)
      .map(r => r.policy!.name);
}

function getVisibleHeaders(): string[] {
  return visibleClasses(
      getChromePolicyTable()!.shadowRoot, 'policy-table-header-cell');
}

function getVisibleCells(name: string): string[] {
  return visibleClasses(
      getChromePolicyRow(name)!.shadowRoot, '.policy.row > *');
}

function getVisibleDetailRows(name: string): string[] {
  return visibleClasses(
      getChromePolicyRow(name)!.shadowRoot, '.row:not(.policy.row)');
}

function getCheckedColumns(): string[] {
  const app = document.querySelector('policy-app')!;
  const checkboxes = app.shadowRoot.querySelectorAll<CrCheckboxElement>(
      '#columnsMenu cr-checkbox.dropdown-item');
  return Array.from(checkboxes)
      .filter(cb => cb.checked)
      .map(cb => cb.dataset['column']!);
}

function getPolicyRowDivTexts(name: string): string[] {
  return Array.from(
      getAllPolicyRowDivs(getChromePolicyRow(name)!),
      d => (d.textContent ?? '').trim());
}

function getPolicyDetailRowValue(name: string, rowClass: string): string {
  const el = getChromePolicyRow(name)!.shadowRoot.querySelector(
      `.${rowClass}.row .value`);
  return el?.textContent?.trim() ?? '';
}

function getToolbarSearchValue(): string {
  const app = document.querySelector('policy-app')!;
  return app.shadowRoot.querySelector<CrToolbarElement>('cr-toolbar')!
      .getSearchField()
      .getValue();
}

async function setToolbarSearchValue(value: string): Promise<boolean> {
  const app = document.querySelector('policy-app')!;
  const table = getChromePolicyTable()!;
  app.shadowRoot.querySelector<CrToolbarElement>('cr-toolbar')!.getSearchField()
      .setValue(value);
  await app.updateComplete;
  await table.updateComplete;
  return true;
}

async function setColumnCheckbox(
    col: string, checked: boolean): Promise<boolean> {
  const app = document.querySelector('policy-app')!;
  const menu =
      app.shadowRoot.querySelector<CrActionMenuElement>('#columnsMenu')!;
  if (!menu.open) {
    app.shadowRoot.querySelector<HTMLElement>('#columns-button')!.click();
    await app.updateComplete;
  }
  const checkbox =
      app.shadowRoot.querySelector<CrCheckboxElement>(`#column-${col}`)!;
  if (checkbox.checked !== checked) {
    checkbox.click();
    await checkbox.updateComplete;
    await app.updateComplete;
  }
  return menu.open;
}

async function togglePolicyRowExpanded(name: string): Promise<boolean> {
  const row = getChromePolicyRow(name)!;
  row.shadowRoot.querySelector<HTMLElement>('.policy.row .toggle')!.click();
  await row.updateComplete;
  return true;
}

async function sortChromePoliciesBy(col: string): Promise<string> {
  const table = getChromePolicyTable()!;
  const header = table.shadowRoot.querySelector(`.row.header .${col}`)!;
  header.shadowRoot!.querySelector<HTMLElement>(`#${col}-sort-down`)!.click();
  await table.updateComplete;
  return table.mostRecentSortedColumn;
}

const smallScreenMediaQuery = window.matchMedia('(max-width: 768px)');
let smallScreenMediaQueryMatches = smallScreenMediaQuery.matches;
smallScreenMediaQuery.addEventListener('change', e => {
  smallScreenMediaQueryMatches = e.matches;
});

async function waitForSmallScreenMediaQuery(expectedMatches: boolean):
    Promise<boolean> {
  if (smallScreenMediaQueryMatches !== expectedMatches) {
    await new Promise(resolve => {
      smallScreenMediaQuery.addEventListener('change', resolve, {once: true});
    });
  }
  const app = document.querySelector('policy-app')!;
  const table = getChromePolicyTable()!;
  await app.updateComplete;
  await table.updateComplete;
  return smallScreenMediaQueryMatches === expectedMatches;
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
  getChromePolicyTable,
  getChromePolicyRow,
  getVisiblePolicies,
  getVisibleHeaders,
  getVisibleCells,
  getVisibleDetailRows,
  getCheckedColumns,
  getPolicyRowDivTexts,
  getPolicyDetailRowValue,
  getToolbarSearchValue,
  setToolbarSearchValue,
  setColumnCheckbox,
  togglePolicyRowExpanded,
  sortChromePoliciesBy,
  waitForSmallScreenMediaQuery,
  getPrecedenceRowValue,
  getRefreshIntervalEl,
  getReportButtonVisibility,
  reloadPolicies,
});
