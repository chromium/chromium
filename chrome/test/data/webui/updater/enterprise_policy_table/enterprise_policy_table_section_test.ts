// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/enterprise_policy_table/enterprise_policy_table_section.js';

import type {EnterprisePolicyTableSectionElement, RowData} from 'chrome://updater/enterprise_policy_table/enterprise_policy_table_section.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('EnterprisePolicyTableSectionTest', () => {
  let element: EnterprisePolicyTableSectionElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('enterprise-policy-table-section');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('renders rows correctly', async () => {
    const rowData: RowData[] = [
      {
        name: 'Policy1',
        policy: {
          valuesBySource: {
            'Source1': 'Value1',
            'Source2': 'Value2',
          },
          prevailingSource: 'Source1',
        },
        isExpanded: false,
        hasConflict: true,
      },
      {
        name: 'Policy2',
        policy: {
          valuesBySource: {
            'Source3': 123,
          },
          prevailingSource: 'Source3',
        },
        isExpanded: true,
        hasConflict: false,
      },
    ];
    element.rowData = rowData;
    await microtasksFinished();

    const rows = element.shadowRoot.querySelectorAll('.policy-row');
    assertEquals(2, rows.length);

    const row1 = rows[0]!;
    assertEquals(
        'Policy1', row1.querySelector('.column-name')!.textContent.trim());
    assertEquals(
        'Source1', row1.querySelector('.column-source')!.textContent.trim());
    assertTrue(!!row1.querySelector('.warning-icon'));
    assertFalse(!!row1.querySelector('.check-icon'));
    assertFalse(row1.hasAttribute('expanded'));
    assertTrue(!!row1.querySelector('.expand-icon'));
    const value1 = row1.querySelector('enterprise-policy-value');
    assertTrue(!!value1);
    assertEquals('Policy1', value1.policyName);
    assertEquals('Value1', value1.value);

    const row2 = rows[1]!;
    assertEquals(
        'Policy2', row2.querySelector('.column-name')!.textContent.trim());
    assertEquals(
        'Source3', row2.querySelector('.column-source')!.textContent.trim());
    assertFalse(!!row2.querySelector('.warning-icon'));
    assertTrue(!!row2.querySelector('.check-icon'));
    assertTrue(row2.hasAttribute('expanded'));
    assertFalse(!!row2.querySelector('.expand-icon'));
    const value2 = row2.querySelector('enterprise-policy-value');
    assertTrue(!!value2);
    assertEquals('Policy2', value2.policyName);
    assertEquals(123, value2.value);

    const expandedSections =
        element.shadowRoot.querySelectorAll('.expanded-section');
    assertEquals(2, expandedSections.length);
    assertFalse(expandedSections[0]!.hasAttribute('opened'));
    assertTrue(expandedSections[1]!.hasAttribute('opened'));
  });

  test('renders empty rowData', async () => {
    element.rowData = [];
    await microtasksFinished();
    assertEquals(0, element.shadowRoot.querySelectorAll('.policy-row').length);
    assertTrue(element.shadowRoot.querySelector('.header-row') !== null);
  });

  test('toggles expansion', async () => {
    element.rowData = [{
      name: 'Policy1',
      policy: {
        valuesBySource: {
          'Source1': 'Value1',
          'Source2': 'Value2',
        },
        prevailingSource: 'Source1',
      },
      isExpanded: false,
      hasConflict: false,
    }];
    await microtasksFinished();

    const expandButton = element.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button.expand-icon')!;
    const collapse = element.shadowRoot.querySelector('cr-collapse')!;

    assertFalse(collapse.opened);
    assertFalse(element.rowData[0]!.isExpanded);

    expandButton.click();
    await microtasksFinished();

    assertTrue(collapse.opened);
    assertTrue(element.rowData[0]!.isExpanded);

    expandButton.click();
    await microtasksFinished();

    assertFalse(collapse.opened);
    assertFalse(element.rowData[0]!.isExpanded);
  });

  test('displays ignored sources in expanded row', async () => {
    element.rowData = [{
      name: 'Policy1',
      policy: {
        valuesBySource: {
          'Source1': 'Value1',
          'Source2': 'Value2',
        },
        prevailingSource: 'Source1',
      },
      isExpanded: true,
      hasConflict: true,
    }];
    await microtasksFinished();

    const expandedRows = element.shadowRoot.querySelectorAll('.expanded-row');
    assertEquals(1, expandedRows.length);

    const row = expandedRows[0]!;
    assertEquals(
        'Ignored:', row.querySelector('.column-name')!.textContent.trim());
    assertEquals(
        'Source2', row.querySelector('.column-source')!.textContent.trim());
    const value = row.querySelector('enterprise-policy-value');
    assertTrue(!!value);
    assertEquals('Policy1', value.policyName);
    assertEquals('Value2', value.value);
  });
});
