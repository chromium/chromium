// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AppManagementToggleRowElement} from 'chrome://os-settings/os_settings.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {replaceBody} from './test_util.js';

suite('<app-management-toggle-row', () => {
  let toggleRow: AppManagementToggleRowElement;

  setup(async () => {
    toggleRow = document.createElement('app-management-toggle-row');
    replaceBody(toggleRow);
    await flushTasks();
  });

  teardown(() => {
    toggleRow.remove();
  });

  test('Click toggle', async () => {
    toggleRow.setToggle(false);
    assertFalse(toggleRow.isChecked());
    toggleRow.click();
    await flushTasks();
    assertTrue(toggleRow.isChecked());
  });

  test('Toggle disabled by policy', async () => {
    toggleRow.setToggle(false);
    assertFalse(toggleRow.isChecked());
    let crToggle = toggleRow.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!crToggle);
    assertFalse(crToggle.disabled);
    assertNull(toggleRow.shadowRoot!.querySelector('cr-policy-indicator'));

    toggleRow.managed = true;
    await flushTasks();
    crToggle = toggleRow.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!crToggle);
    assertTrue(crToggle.disabled);
    assertTrue(!!toggleRow.shadowRoot!.querySelector('cr-policy-indicator'));

    toggleRow.click();
    await flushTasks();
    assertFalse(toggleRow.isChecked());
  });
});
