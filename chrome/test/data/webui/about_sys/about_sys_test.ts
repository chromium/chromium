// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://system/app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SystemAppElement} from 'chrome://system/app.js';
import {BrowserProxyImpl} from 'chrome://system/browser_proxy.js';
import type {SystemLog} from 'chrome://system/browser_proxy.js';
import {COLLAPSE_THRESHOLD} from 'chrome://system/log_entry.js';
import type {LogEntryElement} from 'chrome://system/log_entry.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestAboutSysBrowserProxy} from './test_about_sys_browser_proxy.js';

export const SYSTEM_LOGS: SystemLog[] = [
  {statName: 'CHROME VERSION', 'statValue': '122.0.6261.94'},
  {statName: 'OS VERSION', 'statValue': 'Linux: 6.5.13-1rodete2-amd64'},
  {statName: 'Related Website Sets', 'statValue': 'Disabled'},
  {
    statName: 'device_event_log',
    statValue: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {
    statName: 'extensions',
    statValue: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {statName: 'graphite_enabled', 'statValue': 'false'},
  {
    statName: 'mem_usage',
    statValue: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {
    statName: 'mem_usage_with_title',
    statValue: 't'.repeat(COLLAPSE_THRESHOLD + 1),
  },
  {statName: 'network_event_log', 'statValue': ''},
];

async function createSystemAppElement(): Promise<SystemAppElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const app: SystemAppElement = document.createElement('system-app');
  document.body.appendChild(app);
  await eventToPromise('ready-for-testing', app);
  return app;
}

suite('SystemUITest', function() {
  let app: SystemAppElement;
  let browserProxy: TestAboutSysBrowserProxy;
  let collapsbileLogs: LogEntryElement[];

  function getCollapsibleLogs(): LogEntryElement[] {
    const logs = Array.from(
        app.shadowRoot!.querySelectorAll<LogEntryElement>('log-entry'));
    return logs.filter(log => {
      const button = log.shadowRoot!.querySelector('button');
      return button && !button.hidden;
    });
  }

  setup(async function() {
    browserProxy = new TestAboutSysBrowserProxy();
    browserProxy.setSystemLogs(SYSTEM_LOGS);
    BrowserProxyImpl.setInstance(browserProxy);
    app = await createSystemAppElement();
    collapsbileLogs = getCollapsibleLogs();
  });

  test('Layout', function() {
    // Title
    assertTrue(isVisible(app.$.title));

    // Table title, expand all button, collapse all button, loading status
    assertTrue(isVisible(app.$.tableTitle));
    assertEquals(
        loadTimeData.getString('tableTitle'), app.$.tableTitle.textContent);
    assertTrue(isVisible(app.$.expandAll));
    assertTrue(isVisible(app.$.collapseAll));
    assertFalse(isVisible(app.$.status));

    // Log table
    assertTrue(isVisible(app.$.logTable));
  });

  function expandFirstAndLastCollapsibleLogEntries() {
    assertTrue(collapsbileLogs.length > 0);
    collapsbileLogs[0]!.collapsed = false;
    collapsbileLogs[collapsbileLogs.length - 1]!.collapsed = false;
  }

  test('ExpandAll button expands all collapsible cells', function() {
    expandFirstAndLastCollapsibleLogEntries();

    app.$.expandAll.click();

    for (const log of collapsbileLogs) {
      assertFalse(log.collapsed);
    }
  });

  test('CollapseAll button collapses all collapsible cells', function() {
    expandFirstAndLastCollapsibleLogEntries();

    app.$.collapseAll.click();

    for (const log of collapsbileLogs) {
      assertTrue(log.collapsed);
    }
  });
});

suite('AboutSystemUrlTest', function() {
  let browserProxy: TestAboutSysBrowserProxy;
  let app: SystemAppElement;

  setup(async function() {
    browserProxy = new TestAboutSysBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    app = await createSystemAppElement();
  });

  test('RequestAboutSystemInfoTest', function() {
    return browserProxy.whenCalled('requestSystemInfo');
  });

  test('AboutSystemInfoTitleTest', function() {
    const expectedTitle = loadTimeData.getString('aboutSysTitle');
    assertEquals(expectedTitle, document.title);
    assertEquals(expectedTitle, app.$.title.textContent);
  });
});

suite('FeedbackSysInfoUrlTest', function() {
  let browserProxy: TestAboutSysBrowserProxy;
  let app: SystemAppElement;

  setup(async function() {
    browserProxy = new TestAboutSysBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    app = await createSystemAppElement();
  });

  test('RequestFeedbackSystemInfoTest', function() {
    return browserProxy.whenCalled('requestFeedbackSystemInfo');
  });

  test('FeedbackSystemInfoTitleTest', function() {
    const expectedTitle = loadTimeData.getString('feedbackInfoTitle');
    assertEquals(expectedTitle, document.title);
    assertEquals(expectedTitle, app.$.title.textContent);
  });
});
