// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {replaceBody} from './test_util.m.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

'use strict';

suite('<app-management-toggle-row', () => {
  let toggleRow;

  setup(async () => {
    toggleRow = document.createElement('app-management-toggle-row');
    replaceBody(toggleRow);
    await test_util.flushTasks();
  });

  test('Click toggle', async () => {
    toggleRow.setToggle(false);
    assertFalse(toggleRow.isChecked());
    toggleRow.click();
    await test_util.flushTasks();
    assertTrue(toggleRow.isChecked());
  });

  test('Toggle disabled by policy', async () => {
    toggleRow.setToggle(false);
    assertFalse(toggleRow.isChecked());
    assertFalse(!!toggleRow.$$('cr-toggle').disabled);
    assertFalse(!!toggleRow.$$('cr-policy-indicator'));

    toggleRow.managed = true;
    await test_util.flushTasks();
    assertTrue(!!toggleRow.$$('cr-toggle').disabled);
    assertTrue(!!toggleRow.$$('cr-policy-indicator'));

    toggleRow.click();
    await test_util.flushTasks();
    assertFalse(toggleRow.isChecked());
  });
});
