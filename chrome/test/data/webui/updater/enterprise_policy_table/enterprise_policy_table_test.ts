// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/enterprise_policy_table/enterprise_policy_table.js';

import type {EnterprisePolicyTableElement} from 'chrome://updater/enterprise_policy_table/enterprise_policy_table.js';
import {loadTimeData} from 'chrome://updater/i18n_setup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('EnterprisePolicyTableTest', () => {
  let element: EnterprisePolicyTableElement;

  setup(async () => {
    loadTimeData.overrideValues({
      numKnownApps: 1,
      knownAppName0: 'Chrome',
      knownAppIds0: '{8A69D345-D564-463c-AFF1-A69D9E530F96}',
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('enterprise-policy-table');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('displays message when no policies are set', async () => {
    element.policies = undefined;
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('.no-policies'));
    assertEquals(0, element.shadowRoot.querySelectorAll('.section').length);

    element.policies = {
      policiesByName: {},
      policiesByAppId: {},
    };
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('.no-policies'));
    assertEquals(0, element.shadowRoot.querySelectorAll('.section').length);
  });

  test('displays message when only default values are set', async () => {
    element.policies = {
      policiesByName: {
        'Policy1': {
          valuesBySource: {'Default': 'Value1'},
          prevailingSource: 'Default',
        },
      },
      policiesByAppId: {
        'app1': {
          'Policy2': {
            valuesBySource: {'default': 'Value2'},
            prevailingSource: 'default',
          },
        },
      },
    };
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('.no-policies'));
    assertEquals(0, element.shadowRoot.querySelectorAll('.section').length);
  });

  test('displays message when no policies match appId filter', async () => {
    element.policies = {
      policiesByName: {},
      policiesByAppId: {
        'app1': {
          'Policy1': {
            valuesBySource: {'Cloud': 'Value1'},
            prevailingSource: 'Cloud',
          },
        },
        'app2': {
          'Policy2': {
            valuesBySource: {'Default': 'Value2'},
            prevailingSource: 'Default',
          },
        },
      },
    };
    element.appId = 'app2';
    await microtasksFinished();

    assertTrue(!!element.shadowRoot.querySelector('.no-policies'));
    assertEquals(0, element.shadowRoot.querySelectorAll('.section').length);
  });

  test('renders updater policies', async () => {
    element.policies = {
      policiesByName: {
        'UpdaterPolicy': {
          valuesBySource: {'Cloud': 'Value1'},
          prevailingSource: 'Cloud',
        },
      },
      policiesByAppId: {},
    };
    await microtasksFinished();
    assertFalse(!!element.shadowRoot.querySelector('.no-policies'));

    const sections = element.shadowRoot.querySelectorAll('.section');
    assertEquals(1, sections.length);
    const tableSection =
        sections[0]!.querySelector('enterprise-policy-table-section');
    assertTrue(!!tableSection);
    assertEquals(1, tableSection.rowData.length);
    assertDeepEquals(
        {
          name: 'UpdaterPolicy',
          policy: {
            valuesBySource: {'Cloud': 'Value1'},
            prevailingSource: 'Cloud',
          },
          isExpanded: false,
          hasConflict: false,
        },
        tableSection.rowData[0]);
  });

  test('renders app policies', async () => {
    element.policies = {
      policiesByName: {},
      policiesByAppId: {
        '{8A69D345-D564-463c-AFF1-A69D9E530F96}': {
          'AppPolicy': {
            valuesBySource: {'Platform': 'Value2'},
            prevailingSource: 'Platform',
          },
        },
        'unknown-app': {
          'OtherPolicy': {
            valuesBySource: {'Cloud': 'Value3'},
            prevailingSource: 'Cloud',
          },
        },
      },
    };
    await microtasksFinished();
    assertFalse(!!element.shadowRoot.querySelector('.no-policies'));

    const sections = element.shadowRoot.querySelectorAll('.section');
    assertEquals(2, sections.length);
    assertEquals(
        'Chrome policies',
        sections[0]!.querySelector('h3')!.textContent.trim());
    assertEquals(
        'unknown-app policies',
        sections[1]!.querySelector('h3')!.textContent.trim());
  });

  test('filters by appId', async () => {
    element.policies = {
      policiesByName: {
        'UpdaterPolicy': {
          valuesBySource: {'Cloud': 'Value1'},
          prevailingSource: 'Cloud',
        },
      },
      policiesByAppId: {
        'app1': {
          'App1Policy': {
            valuesBySource: {'Platform': 'Value2'},
            prevailingSource: 'Platform',
          },
        },
        'app2': {
          'App2Policy': {
            valuesBySource: {'Cloud': 'Value3'},
            prevailingSource: 'Cloud',
          },
        },
      },
    };
    element.appId = 'app1';
    await microtasksFinished();

    const sections = element.shadowRoot.querySelectorAll('.section');
    assertEquals(2, sections.length);
    assertEquals(
        'Updater policies',
        sections[0]!.querySelector('h3')!.textContent.trim());
    assertEquals(
        'app1 policies', sections[1]!.querySelector('h3')!.textContent.trim());
  });

  test('filters by case insensitive appId', async () => {
    element.policies = {
      policiesByName: {},
      policiesByAppId: {
        'APP1': {
          'App1Policy': {
            valuesBySource: {'Platform': 'Value2'},
            prevailingSource: 'Platform',
          },
        },
      },
    };
    element.appId = 'app1';
    await microtasksFinished();

    const sections = element.shadowRoot.querySelectorAll('.section');
    assertEquals(1, sections.length);
    assertEquals(
        'APP1 policies', sections[0]!.querySelector('h3')!.textContent.trim());
  });

  test('computes hasConflict', async () => {
    element.policies = {
      policiesByName: {
        'ConflictingPolicy': {
          valuesBySource: {
            'Cloud': 'Value1',
            'Platform': 'Value2',
          },
          prevailingSource: 'Cloud',
        },
        'MultipleMatchingNonDefault': {
          valuesBySource: {
            'Cloud': 'Value2',
            'Platform': 'Value2',
            'Default': 'Value1',
          },
          prevailingSource: 'Cloud',
        },
        'SingleNonDefault': {
          valuesBySource: {
            'Cloud': 'Value1',
            'Default': 'Value2',
          },
          prevailingSource: 'Cloud',
        },
      },
      policiesByAppId: {},
    };
    await microtasksFinished();

    const tableSection =
        element.shadowRoot.querySelector('enterprise-policy-table-section')!;
    const rowData = tableSection.rowData;
    assertEquals(3, rowData.length);

    assertTrue(rowData.find(r => r.name === 'ConflictingPolicy')!.hasConflict);
    assertFalse(
        rowData.find(
                   r => r.name === 'MultipleMatchingNonDefault')!.hasConflict);
    assertFalse(rowData.find(r => r.name === 'SingleNonDefault')!.hasConflict);
  });
});
