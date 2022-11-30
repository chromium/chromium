// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import 'chrome://os-settings/chromeos/os_settings.js';

import {replaceBody} from './test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<app-management-toggle-row', () => {
  let toggleRow;

  setup(async () => {
    toggleRow = document.createElement('app-management-toggle-row');
    replaceBody(toggleRow);
    await flushTasks();
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
    assertFalse(!!toggleRow.shadowRoot.querySelector('cr-toggle').disabled);
    assertFalse(!!toggleRow.shadowRoot.querySelector('cr-policy-indicator'));

    toggleRow.managed = true;
    await flushTasks();
    assertTrue(!!toggleRow.shadowRoot.querySelector('cr-toggle').disabled);
    assertTrue(!!toggleRow.shadowRoot.querySelector('cr-policy-indicator'));

    toggleRow.click();
    await flushTasks();
    assertFalse(toggleRow.isChecked());
  });
});
