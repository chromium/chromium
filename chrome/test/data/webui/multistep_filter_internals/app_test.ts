// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://multistep-filter-internals/app.js';

import type {MultistepFilterInternalsAppElement} from 'chrome://multistep-filter-internals/app.js';
import {browserProxyFactory} from 'chrome://multistep-filter-internals/multistep_filter_internals.mojom-webui.js';
import type {LogEntry, PageHandlerInterface, PageRemote} from 'chrome://multistep-filter-internals/multistep_filter_internals.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

const TEST_NAV_ID = 123456n;

class TestPageHandler extends TestBrowserProxy implements PageHandlerInterface {
  constructor() {
    super(['getBufferedLogs']);
  }

  getBufferedLogs() {
    this.methodCalled('getBufferedLogs');
    return Promise.resolve({logs: []});
  }
}

suite('AppTest', function() {
  let app: MultistepFilterInternalsAppElement;
  let pageRemote: PageRemote;

  function fireLogEntryAdded(mojoLog: LogEntry) {
    pageRemote.onLogEntryAdded(mojoLog);
    return pageRemote.$.flushForTesting();
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const handler = new TestPageHandler();
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    pageRemote = remote;

    app = document.createElement('multistep-filter-internals-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('Page loads and list exists', function() {
    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
  });

  test('List is populated with logs', async function() {
    const mojoLog: LogEntry = {
      timestamp: {internalValue: 13350000000000000n},
      eventType: 'Navigation Started',
      host: 'example.com',
      navigationId: TEST_NAV_ID,
      details: 'key: foo',
    };

    await fireLogEntryAdded(mojoLog);
    await microtasksFinished();

    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
    const lines = list.querySelectorAll('.log-line');
    assertEquals(1, lines.length);

    const line = lines[0]!;
    assertEquals(
        'Navigation Started',
        line.querySelector('.text-event')!.textContent?.trim());
    assertEquals(
        'example.com', line.querySelector('.text-host')!.textContent?.trim());
    assertEquals(
        `[${TEST_NAV_ID.toString()}]`,
        line.querySelector('.text-nav')!.textContent?.trim());
    const detailsText = line.querySelector('.text-details')?.textContent || '';
    assertTrue(detailsText.includes('foo'));
  });

  test('Search filter works', async function() {
    await fireLogEntryAdded({
      timestamp: {internalValue: 13350000000000000n},
      eventType: 'Url Eligibility Check',
      host: 'apple.com',
      navigationId: TEST_NAV_ID,
      details: '',
    });
    await fireLogEntryAdded({
      timestamp: {internalValue: 13350000000000001n},
      eventType: 'Annotation Extraction Started',
      host: 'banana.com',
      navigationId: TEST_NAV_ID,
      details: '',
    });
    await microtasksFinished();

    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
    let lines = list.querySelectorAll('.log-line');
    assertEquals(2, lines.length);

    const searchInput =
        app.shadowRoot.querySelector<HTMLInputElement>('#filter-input');
    assertTrue(!!searchInput);
    searchInput.value = 'eligibility';
    searchInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    lines = list.querySelectorAll('.log-line');
    assertEquals(1, lines.length);
    const line = lines[0]!;
    assertEquals(
        'Url Eligibility Check',
        line.querySelector('.text-event')!.textContent?.trim());
  });

  test('Deduplication ignores identical timestamps', async function() {
    const rawLog: LogEntry = {
      timestamp: {internalValue: 13350000000000000n},
      eventType: 'Navigation Started',
      navigationId: TEST_NAV_ID,
      host: '',
      details: '',
    };

    await fireLogEntryAdded(rawLog);
    await fireLogEntryAdded(rawLog);
    await microtasksFinished();

    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
    assertEquals(1, list.querySelectorAll('.log-line').length);
  });

  test('Search filter matches details', async function() {
    await fireLogEntryAdded({
      timestamp: {internalValue: 13350000000000000n},
      eventType: 'Url Eligibility Check',
      navigationId: TEST_NAV_ID,
      details: 'safe: match_this_string',
      host: '',
    });
    await microtasksFinished();

    const searchInput =
        app.shadowRoot.querySelector<HTMLInputElement>('#filter-input');
    assertTrue(!!searchInput);
    searchInput.value = 'match_this_string';
    searchInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
    assertEquals(1, list.querySelectorAll('.log-line').length);
  });

  test('Clear logs button works', async function() {
    await fireLogEntryAdded({
      timestamp: {internalValue: 13350000000000000n},
      eventType: 'Url Eligibility Check',
      navigationId: TEST_NAV_ID,
      host: '',
      details: '',
    });
    await microtasksFinished();

    const clearBtn = app.shadowRoot.querySelector<HTMLElement>('#clear-btn');
    assertTrue(!!clearBtn);
    clearBtn.click();
    await microtasksFinished();

    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
    assertEquals(0, list.querySelectorAll('.log-line').length);
  });

  test('Logs are prepended (newest at top)', async function() {
    await fireLogEntryAdded({
      timestamp: {internalValue: 1000n},
      eventType: 'Event 1 (Old)',
      navigationId: TEST_NAV_ID,
      host: '',
      details: '',
    });
    await fireLogEntryAdded({
      timestamp: {internalValue: 2000n},
      eventType: 'Event 2 (New)',
      navigationId: TEST_NAV_ID,
      host: '',
      details: '',
    });
    await microtasksFinished();

    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
    const lines = list.querySelectorAll('.log-line');
    assertEquals(2, lines.length);

    assertEquals(
        'Event 2 (New)',
        lines[0]!.querySelector('.text-event')!.textContent?.trim());
    assertEquals(
        'Event 1 (Old)',
        lines[1]!.querySelector('.text-event')!.textContent?.trim());
  });
});

class TestPageHandlerWithLogs extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super(['getBufferedLogs']);
  }

  getBufferedLogs() {
    this.methodCalled('getBufferedLogs');
    return Promise.resolve({
      logs: [
        {
          timestamp: {internalValue: 2000n},
          eventType: 'Buffered Middle',
          host: '',
          navigationId: 0n,
          details: '',
        },
        {
          timestamp: {internalValue: 1000n},
          eventType: 'Buffered Oldest',
          host: '',
          navigationId: 0n,
          details: '',
        },
        {
          timestamp: {internalValue: 3000n},
          eventType: 'Buffered Newest',
          host: '',
          navigationId: 0n,
          details: '',
        },
      ],
    });
  }
}

suite('AppTestWithBufferedLogs', function() {
  let app: MultistepFilterInternalsAppElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const handler = new TestPageHandlerWithLogs();
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);

    app = document.createElement('multistep-filter-internals-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('Buffered logs are sorted with newest at top', function() {
    const list = app.shadowRoot.querySelector('#log-list');
    assertTrue(!!list);
    const lines = list.querySelectorAll('.log-line');
    assertEquals(3, lines.length);

    assertEquals(
        'Buffered Newest',
        lines[0]!.querySelector('.text-event')!.textContent?.trim());
    assertEquals(
        'Buffered Middle',
        lines[1]!.querySelector('.text-event')!.textContent?.trim());
    assertEquals(
        'Buffered Oldest',
        lines[2]!.querySelector('.text-event')!.textContent?.trim());
  });
});
