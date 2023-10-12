// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {assertGT} from 'chrome://webui-test/chai_assert.js';

/**
 * Gets the column text contents of a specific row of a table body element.
 */
export function getTableRowAsStringArray(
    table: HTMLElement, row: number): string[] {
  const rows = table.querySelectorAll('tr');
  assertGT(rows.length, row);
  const rowEl = rows[row];
  assert(rowEl);
  return Array.from(rowEl.querySelectorAll('td')).map(el => el.innerText);
}
