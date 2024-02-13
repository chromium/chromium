// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://metrics-internals/app.js';

import type {MetricsInternalsAppElement} from 'chrome://metrics-internals/app.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getTableRowAsStringArray} from './utils.js';

suite('NoLogsModuleTest', function() {
  let app: MetricsInternalsAppElement;

  setup(() => {
    app = document.createElement('metrics-internals-app');
    document.body.appendChild(app);
    return app.initPromise;
  });

  test('UMA summary table should have info', function() {
    const umaSummaryTable =
        app.shadowRoot!.querySelector<HTMLElement>('#uma-summary-body');
    assert(umaSummaryTable);
    const rows = umaSummaryTable.querySelectorAll('tr');

    // The table should be non-empty. Test for a few rows that should be there.
    assertGT(rows.length, 0);
    const rowsKeys = Array.from(rows).map(
        el => (el!.firstElementChild as HTMLElement).innerText);
    assertTrue(rowsKeys.includes('Client ID'));
    assertTrue(rowsKeys.includes('Metrics Reporting Enabled'));
  });

  test('variations summary table should have info', function() {
    const variationsSummaryTable =
        app.shadowRoot!.querySelector<HTMLElement>('#variations-summary-body');
    assert(variationsSummaryTable);
    const rows = variationsSummaryTable.querySelectorAll('tr');

    // The table should be non-empty. Test for a few rows that should be
    // there.
    assertGT(rows.length, 0);
    const rowsKeys = Array.from(rows).map(
        el => (el!.firstElementChild as HTMLElement).innerText);
    assertTrue(rowsKeys.includes('Channel'));
    assertTrue(rowsKeys.includes('Version'));
    assertTrue(rowsKeys.includes('Platform'));
  });

  test('table should show an empty log if there are no logs', function() {
    const umaLogsTable =
        app.shadowRoot!.querySelector<HTMLElement>('#uma-logs-body');
    assert(umaLogsTable);

    // All 5 columns of the first row should be filled with 'N/A'.
    const firstRow = getTableRowAsStringArray(umaLogsTable, 0);
    assertEquals(firstRow.length, 5);
    firstRow.forEach((el: string) => {
      assertEquals(el, 'N/A');
    });
  });

  test('exported logs should be empty if there are no logs', async function() {
    const exportedLogs = await app.getUmaLogsExportContent();
    const exportedLogsObj = JSON.parse(exportedLogs);

    // The exported logs should contain no logs.
    assertEquals(exportedLogsObj.logType, 'UMA');
    assertTrue(!!exportedLogsObj.logs);
    assertEquals(exportedLogsObj.logs.length, 0);
  });
});
