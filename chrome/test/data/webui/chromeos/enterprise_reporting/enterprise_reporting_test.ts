// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Unittests for the chrome://enterprise-reporting element.
 */

import {EnterpriseReportingBrowserProxy} from 'chrome://enterprise-reporting/browser_proxy.js';
import {ErpHistoryData, ErpHistoryEvent, ErpHistoryEventParameter, PageCallbackRouter, PageHandlerRemote, PageRemote} from 'chrome://enterprise-reporting/enterprise_reporting.mojom-webui.js';
import {ReportingHistoryElement} from 'chrome://enterprise-reporting/reporting_history.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('enterprise_reporting', function() {
  let reportingHistoryElement: ReportingHistoryElement;
  let callbackRouterRemote: PageRemote;
  let handler: TestMock<PageHandlerRemote>;

  // Number of cells in HTML row representing a single event, produced by
  // `chrome://enterprise_reporting/reporting_history.ts`:
  // `call`, `parameters`, `status` and `timestamp`.
  const numCellsInRow = 4;

  type Constructor<T> = new (...args: any[]) => T;
  type Installer<T> = (instance: T) => void;

  function installMock<T extends object>(
      clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
    installer = installer ||
        (clazz as unknown as {setInstance: Installer<T>}).setInstance;
    const mock = TestMock.fromClass(clazz);
    installer!(mock);
    return mock;
  }

  function getHistoryTable(): HTMLElement {
    return reportingHistoryElement.$.body;
  }

  function timestampToString(timestampSeconds: bigint): string {
    // Multiply by 1000 since the constructor expects milliseconds, but the
    // timestamps are in seconds.
    const timestamp: Date = new Date(Number(timestampSeconds) * 1000);

    // For today's timestamp, show time only.
    const now: Date = new Date();
    if (timestamp.getDate() === now.getDate()) {
      return timestamp.toLocaleTimeString();
    }

    // Otherwise show whole timestamp.
    return timestamp.toLocaleString();
  }

  function parametersMatch(
      expectedParameters: ErpHistoryEventParameter[],
      parameters: HTMLTableCellElement) {
    const lines = parameters.querySelectorAll<HTMLLIElement>('li');
    assertEquals(expectedParameters.length, lines.length);
    lines.forEach((line, index) => {
      assertEquals(
          (expectedParameters[index]!.name + ': ' +
           expectedParameters[index]!.value),
          line.innerText);
    });
  }

  function cellMatches(
      expectedEvent: ErpHistoryEvent, row: HTMLTableRowElement) {
    // Enumerate and match cells in the row.
    const cells = row.querySelectorAll<HTMLTableCellElement>('td');
    assertEquals(numCellsInRow, cells.length);
    assertEquals(expectedEvent.call, cells[0]!.innerText);
    parametersMatch(expectedEvent.parameters, cells[1]!);
    assertEquals(expectedEvent.status, cells[2]!.innerText);
    assertEquals(timestampToString(expectedEvent.time), cells[3]!.innerText);
  }

  function rowsMatchInReverse(
      expectedHistory: ErpHistoryData, rows: NodeListOf<HTMLTableRowElement>) {
    // Enumerate and match all rows.
    assertEquals(expectedHistory.events.length, rows.length);
    rows.forEach((row, index) => {
      cellMatches(
          expectedHistory.events[expectedHistory.events.length - index - 1]!,
          row);
    });
  }

  function emptyMatch(rows: NodeListOf<HTMLTableRowElement>) {
    // Check for empty case.
    assertEquals(1, rows.length);
    // Enumerate the cells.
    const cells = rows[0]!.querySelectorAll<HTMLTableCellElement>('td');
    assertEquals(numCellsInRow, cells.length);
    assertEquals('No events', cells[0]!.innerText);
    assertEquals('', cells[1]!.innerText);
    assertEquals('', cells[2]!.innerText);
    assertEquals('', cells[3]!.innerText);
  }

  async function setInitialSettings() {
    callbackRouterRemote.setErpHistoryData({events: []});
    handler.setResultFor('getDebugState', Promise.resolve({state: true}));
    handler.setResultFor('getErpHistoryData', Promise.resolve({events: []}));
    reportingHistoryElement =
        document.createElement(ReportingHistoryElement.is);
    document.body.appendChild(reportingHistoryElement);
    await handler.whenCalled('getDebugState');
    await handler.whenCalled('getErpHistoryData');
  }

  setup(() => {
    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            EnterpriseReportingBrowserProxy.createInstanceForTest(
                mock, new PageCallbackRouter()));
    callbackRouterRemote = EnterpriseReportingBrowserProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  teardown(async () => {
    await callbackRouterRemote.$.flushForTesting();
    await flushTasks();
    reportingHistoryElement.remove();
  });

  test('create History Element and see that it is empty', async () => {
    await setInitialSettings();
    await callbackRouterRemote.$.flushForTesting();
    await handler.whenCalled('getErpHistoryData');
    await flushTasks();

    const table = getHistoryTable();
    emptyMatch(table.querySelectorAll<HTMLTableRowElement>('tr'));
  });

  test('create History Element and update it with data', async () => {
    await setInitialSettings();
    await callbackRouterRemote.$.flushForTesting();

    const event1: ErpHistoryEvent = {
      call: 'call',
      parameters: [{name: 'seq_id', value: '345'} as ErpHistoryEventParameter],
      status: 'OK',
      time: BigInt(123456789),
    };
    const event2: ErpHistoryEvent = {
      call: 'recall',
      parameters: [
        {name: 'seq_id', value: '123'} as ErpHistoryEventParameter,
        {name: 'count', value: '777'} as ErpHistoryEventParameter,
      ],
      status: 'Error',
      time: BigInt(987654321),
    };
    const event3: ErpHistoryEvent = {
      call: 'upload',
      parameters: [
        {name: 'seq_id', value: '123'} as ErpHistoryEventParameter,
        {name: 'seq_id', value: '456'} as ErpHistoryEventParameter,
        {name: 'seq_id', value: '789'} as ErpHistoryEventParameter,
      ],
      status: 'Success',
      time: BigInt(555666777),
    };
    const history: ErpHistoryData = {events: [event1, event2, event3]};
    callbackRouterRemote.setErpHistoryData(history);
    await handler.whenCalled('getErpHistoryData');
    await flushTasks();

    const table = getHistoryTable();
    rowsMatchInReverse(
        history, table.querySelectorAll<HTMLTableRowElement>('tr'));
  });
});
