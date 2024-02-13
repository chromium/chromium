// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://metrics-internals/app.js';

import type {MetricsInternalsAppElement} from 'chrome://metrics-internals/app.js';
import {getEventsPeekString, sizeToString, timestampToString} from 'chrome://metrics-internals/log_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getTableRowAsStringArray} from './utils.js';

suite('WithLogModuleTest', function() {
  let app: MetricsInternalsAppElement;

  setup(() => {
    app = document.createElement('metrics-internals-app');
    document.body.appendChild(app);
    return app.initPromise;
  });

  test('table should show log info if there is one', function() {
    const umaLogsTable =
        app.shadowRoot!.querySelector<HTMLElement>('#uma-logs-body');
    assert(umaLogsTable);

    // None of the 5 columns of the first row should be filled with 'N/A' or
    // be empty.
    const firstRow = getTableRowAsStringArray(umaLogsTable, 0);
    assertEquals(firstRow.length, 5);
    firstRow.forEach((el: string) => {
      assertNotEquals(el, 'N/A');
      assertGT(el.length, 0);
    });
  });

  test('Exported logs should match existing logs', async function() {
    const exportedLogs = await app.getUmaLogsExportContent();
    const exportedLogsObj = JSON.parse(exportedLogs);

    // There should be one log in the exported logs.
    assertEquals(exportedLogsObj.logType, 'UMA');
    assertTrue(!!exportedLogsObj.logs);
    assertEquals(exportedLogsObj.logs.length, 1);

    const exportedLog = exportedLogsObj.logs[0];
    const umaLogsTable =
        app.shadowRoot!.querySelector<HTMLElement>('#uma-logs-body');
    assert(umaLogsTable);
    const firstRow = getTableRowAsStringArray(umaLogsTable, 0);

    // Verify that the exported log matches what is displayed on the page.
    assertTrue(!!exportedLog.type);
    assertEquals(exportedLog.type, firstRow[0]);

    assertTrue(!!exportedLog.hash);
    assertEquals(exportedLog.hash, firstRow[1]);

    assertTrue(!!exportedLog.timestamp);
    assertEquals(timestampToString(exportedLog.timestamp), firstRow[2]);

    assertTrue(!!exportedLog.size);
    assertEquals(sizeToString(exportedLog.size), firstRow[3]);

    assertTrue(!!exportedLog.events);
    assertGT(exportedLog.events.length, 0);
    assertEquals(getEventsPeekString(exportedLog.events), firstRow[4]);

    // Verify that the exported log has proto data.
    assertTrue(!!exportedLog.data);
    assertGT(exportedLog.data.length, 0);
  });
});
