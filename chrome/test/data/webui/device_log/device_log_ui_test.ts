// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://device-log/app.js';

import type {DeviceLogAppElement} from 'chrome://device-log/app.js';
import type {BrowserProxy, LogEntry} from 'chrome://device-log/browser_proxy.js';
import {BrowserProxyImpl, LogLevel} from 'chrome://device-log/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestBrowserProxy extends BaseTestBrowserProxy implements BrowserProxy {
  private logs_: string = '';

  constructor() {
    super(['getLog', 'clearLog']);
  }

  getLog(): Promise<string> {
    this.methodCalled('getLog');
    return Promise.resolve(this.logs_);
  }

  clearLog() {
    this.methodCalled('clearLog');
  }

  setLogs(logs: LogEntry[]) {
    this.logs_ = JSON.stringify(logs.map(item => JSON.stringify(item)));
  }
}

suite('DeviceLog', function() {
  let app: DeviceLogAppElement;
  let browserProxy: TestBrowserProxy;

  const testLogs: LogEntry[] = [
    {
      type: 'Printer',
      level: LogLevel.DEBUG,
      timestamp: '2025-12-19T10:00:01.000Z',
      timestampshort: '10:00:01',
      file: 'printer.cc:2',
      event: 'Ink low',
    },
    {
      type: 'Network',
      level: LogLevel.EVENT,
      timestamp: '2025-12-19T10:00:02.000Z',
      timestampshort: '10:00:02',
      file: 'network.cc:1',
      event: 'Network connected',
    },
    {
      type: 'Printer',
      level: LogLevel.USER,
      timestamp: '2025-12-19T10:00:03.000Z',
      timestampshort: '10:00:03',
      file: 'printer.cc:2',
      event: 'Printing started',
    },
    {
      type: 'Printer',
      level: LogLevel.ERROR,
      timestamp: '2025-12-19T10:00:04.000Z',
      timestampshort: '10:00:04',
      file: 'printer.cc:2',
      event: 'Printer on fire',
    },
  ];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    browserProxy.setLogs(testLogs);

    app = document.createElement('device-log-app');
    document.body.appendChild(app);
    return browserProxy.whenCalled('getLog').then(() => microtasksFinished());
  });

  function getLogEntries(): NodeListOf<HTMLElement> {
    return app.shadowRoot.querySelectorAll<HTMLElement>('#logContainer p');
  }

  test('loads and displays logs', function() {
    const logEntries = getLogEntries();
    assertEquals(testLogs.length, logEntries.length);
    assertTrue(logEntries[0]!.textContent.includes('Printer'));
    assertTrue(logEntries[0]!.textContent.includes('Debug'));
    assertTrue(logEntries[0]!.textContent.includes('10:00:01'));
    assertTrue(logEntries[0]!.textContent.includes('Ink low'));
    // File info is off by default.
    assertFalse(logEntries[0]!.textContent.includes('printer.cc:2'));
  });

  test('refreshes logs', async function() {
    const newLogs = [
      {
        type: 'Usb',
        level: LogLevel.EVENT,
        timestamp: '2025-12-19T10:01:00.000Z',
        timestampshort: '10:01:00.000',
        file: 'fire.cc:5',
        event: 'Firefighters called',
      },
    ];
    browserProxy.setLogs(newLogs);

    app.$.logRefresh.click();

    await browserProxy.whenCalled('getLog');
    await microtasksFinished();

    const logEntries = getLogEntries();
    assertEquals(1, logEntries.length);
    assertTrue(logEntries[0]!.textContent.includes('Usb'));
  });

  test('clears logs', async function() {
    let logEntries = getLogEntries();
    assertEquals(testLogs.length, logEntries.length);

    app.$.logClear.click();

    await browserProxy.whenCalled('clearLog');
    await microtasksFinished();

    logEntries = getLogEntries();
    assertEquals(0, logEntries.length);
    const noEntriesSpan = app.shadowRoot.querySelector('#logContainer span')!;
    assertTrue(noEntriesSpan.textContent.includes(
        loadTimeData.getString('logNoEntriesText')));
  });

  test('filters by log level', async function() {
    app.$.logLevelSelect.value = LogLevel.EVENT;
    app.$.logLevelSelect.dispatchEvent(new Event('change'));
    await microtasksFinished();
    let logEntries = getLogEntries();
    assertEquals(3, logEntries.length);
    assertTrue(logEntries[0]!.textContent.includes(LogLevel.EVENT));
    assertTrue(logEntries[1]!.textContent.includes(LogLevel.USER));
    assertTrue(logEntries[2]!.textContent.includes(LogLevel.ERROR));

    app.$.logLevelSelect.value = LogLevel.USER;
    app.$.logLevelSelect.dispatchEvent(new Event('change'));
    await microtasksFinished();
    logEntries = getLogEntries();
    assertEquals(2, logEntries.length);
    assertTrue(logEntries[0]!.textContent.includes(LogLevel.USER));
    assertTrue(logEntries[1]!.textContent.includes(LogLevel.ERROR));

    app.$.logLevelSelect.value = LogLevel.ERROR;
    app.$.logLevelSelect.dispatchEvent(new Event('change'));
    await microtasksFinished();
    logEntries = getLogEntries();
    assertEquals(1, logEntries.length);
    assertTrue(logEntries[0]!.textContent.includes(LogLevel.ERROR));
  });

  test('filters by log type', async function() {
    const networkCheckbox =
        app.shadowRoot.querySelector<HTMLInputElement>('#logTypeNetwork')!;
    networkCheckbox.checked = false;
    networkCheckbox.dispatchEvent(new Event('change'));
    await microtasksFinished();

    const logEntries = getLogEntries();
    assertEquals(testLogs.length - 1, logEntries.length);
    for (const entry of logEntries) {
      assertFalse(entry.textContent.includes('Network'));
    }
  });

  test('clears log type filters', async function() {
    app.$.logClearTypes.click();
    await microtasksFinished();

    const logEntries = getLogEntries();
    assertEquals(0, logEntries.length);

    const checkboxes = app.shadowRoot.querySelectorAll<HTMLInputElement>(
        '#logCheckboxContainer input[type="checkbox"]');
    for (const checkbox of checkboxes) {
      assertFalse(checkbox.checked);
    }
  });

  test('toggles file info', async function() {
    let logEntry = getLogEntries()[0]!;
    assertFalse(logEntry.textContent.includes('network.cc:1'));

    app.$.logFileinfo.checked = true;
    app.$.logFileinfo.dispatchEvent(new Event('change'));
    await microtasksFinished();

    logEntry = getLogEntries()[0]!;
    assertTrue(logEntry.textContent.includes('printer.cc:2'));

    app.$.logFileinfo.checked = false;
    app.$.logFileinfo.dispatchEvent(new Event('change'));
    await microtasksFinished();

    logEntry = getLogEntries()[0]!;
    assertFalse(logEntry.textContent.includes('printer.cc:2'));
  });

  test('toggles time detail', async function() {
    let logEntry = getLogEntries()[0]!;
    assertTrue(logEntry.textContent.includes('10:00:01'));
    assertFalse(logEntry.textContent.includes('2025-12-19T10:00:01.000Z'));

    app.$.logTimedetail.checked = true;
    app.$.logTimedetail.dispatchEvent(new Event('change'));
    await microtasksFinished();

    logEntry = getLogEntries()[0]!;
    assertTrue(logEntry.textContent.includes('2025-12-19T10:00:01.000Z'));

    app.$.logTimedetail.checked = false;
    app.$.logTimedetail.dispatchEvent(new Event('change'));
    await microtasksFinished();

    logEntry = getLogEntries()[0]!;
    assertTrue(logEntry.textContent.includes('10:00:01'));
    assertFalse(logEntry.textContent.includes('2025-12-19T10:00:01.000Z'));
  });

  test('refreshes logs automatically with url param', async function() {
    const mockTimer = new MockTimer();
    mockTimer.install();
    const microtasksFinishedWithTimer = () => {
      const promise = microtasksFinished();
      mockTimer.tick(0);
      return promise;
    };
    try {
      // The setup function has already created an app instance. We need to
      // create a new one with a different URL.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      window.history.pushState({}, '', '?refresh=1');
      browserProxy.reset();
      browserProxy.setLogs([testLogs[0]!]);
      app = document.createElement('device-log-app');
      document.body.appendChild(app);

      // The app should fetch logs on startup.
      await browserProxy.whenCalled('getLog');
      browserProxy.resetResolver('getLog');
      await microtasksFinishedWithTimer();
      assertEquals(1, getLogEntries().length);

      // After 1 second, it should fetch logs again.
      browserProxy.setLogs([testLogs[0]!, testLogs[1]!]);
      mockTimer.tick(1000);
      await browserProxy.whenCalled('getLog');
      browserProxy.resetResolver('getLog');
      await microtasksFinishedWithTimer();
      assertEquals(2, getLogEntries().length);

      // And again after another second.
      browserProxy.setLogs([testLogs[0]!, testLogs[1]!, testLogs[2]!]);
      mockTimer.tick(1000);
      await browserProxy.whenCalled('getLog');
      await microtasksFinishedWithTimer();
      assertEquals(3, getLogEntries().length);
    } finally {
      mockTimer.uninstall();
      window.history.pushState({}, '', window.location.pathname);
    }
  });

  test('checkboxes toggle based on URL parameters', async function() {
    try {
      // The setup function has already created an app instance. We need to
      // create a new one with a different URL.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      window.history.pushState({}, '', '?types=Network,Usb');
      browserProxy.reset();
      app = document.createElement('device-log-app');
      document.body.appendChild(app);
      await browserProxy.whenCalled('getLog');
      await microtasksFinished();

      const checkboxes = app.shadowRoot.querySelectorAll<HTMLInputElement>(
          '#logCheckboxContainer input[type="checkbox"]');
      checkboxes.forEach((checkbox: HTMLInputElement) => {
        assertEquals(
            checkbox.checked,
            checkbox.value === 'Network' || checkbox.value === 'Usb');
      });
      assertEquals(1, getLogEntries().length);
      assertTrue(getLogEntries()[0]!.textContent.includes('Network'));
    } finally {
      window.history.pushState({}, '', window.location.pathname);
    }
  });
});
