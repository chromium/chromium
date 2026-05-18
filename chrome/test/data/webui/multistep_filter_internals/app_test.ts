// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://multistep-filter-internals/app.js';

import type {MultistepFilterInternalsAppElement} from 'chrome://multistep-filter-internals/app.js';
import {BrowserProxyImpl} from 'chrome://multistep-filter-internals/browser_proxy.js';
import {PageCallbackRouter} from 'chrome://multistep-filter-internals/multistep_filter_internals.mojom-webui.js';
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

class TestMultistepFilterBrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: TestPageHandler = new TestPageHandler();
  pageRemote: PageRemote;

  constructor() {
    this.pageRemote = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

suite('AppTest', function() {
  let app: MultistepFilterInternalsAppElement;
  let browserProxy: TestMultistepFilterBrowserProxy;

  function fireLogEntryAdded(mojoLog: LogEntry) {
    browserProxy.pageRemote.onLogEntryAdded(mojoLog);
    return browserProxy.pageRemote.$.flushForTesting();
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestMultistepFilterBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy as unknown as BrowserProxyImpl);

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
      sourceEtldPlus1: 'example.com',
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
        'example.com', line.querySelector('.text-domain')!.textContent?.trim());
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
      sourceEtldPlus1: 'apple.com',
      navigationId: TEST_NAV_ID,
      details: '',
    });
    await fireLogEntryAdded({
      timestamp: {internalValue: 13350000000000001n},
      eventType: 'Annotation Extraction Started',
      sourceEtldPlus1: 'banana.com',
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
      sourceEtldPlus1: '',
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
      sourceEtldPlus1: '',
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
      sourceEtldPlus1: '',
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
});
