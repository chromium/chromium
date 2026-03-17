// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {SearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

async function createAndAppendRealbox(
    properties: Partial<SearchboxElement> = {}): Promise<SearchboxElement> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const realbox = document.createElement('cr-searchbox');
  Object.assign(realbox, properties);
  document.body.appendChild(realbox);
  await microtasksFinished();
  return realbox;
}

suite('SearchboxFocusTest', () => {
  let realbox: SearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  const testMetricsReporterProxy = TestMock.fromClass(BrowserProxyImpl);

  setup(async () => {
    loadTimeData.overrideValues({
      isLensSearchbox: false,
      searchboxCyclingPlaceholders: false,
      searchboxDefaultIcon: 'search.svg',
      searchboxSeparator: ' - ',
      searchboxVoiceSearch: true,
      reportMetrics: true,
    });

    // Set up Realbox's browser proxy.
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    // Set up MetricsReporter's browser proxy.
    testMetricsReporterProxy.reset();
    const metricsReporterCallbackRouter = new PageMetricsCallbackRouter();
    testMetricsReporterProxy.setResultFor(
        'getCallbackRouter', metricsReporterCallbackRouter);
    testMetricsReporterProxy.setResultFor('getMark', Promise.resolve(null));
    BrowserProxyImpl.setInstance(testMetricsReporterProxy);
    MetricsReporterImpl.setInstanceForTest(new MetricsReporterImpl());

    realbox = await createAndAppendRealbox();
  });

  async function areMatchesShowing(): Promise<boolean> {
    // Force a synchronous render.
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    return window.getComputedStyle(realbox.getDropdownElement()).display !==
        'none';
  }

  test('tabbing with inline autocompletion', async () => {
    realbox.composeButtonEnabled = true;
    realbox.$.input.focus();
    assertEquals(realbox.$.input, realbox.shadowRoot.activeElement);

    realbox.$.input.inputElement.value = 'goo';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, realbox.$.input.inputElement.value, 'input');
    testProxy.handler.reset();

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'gle',
    })];

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing(), 'matches showing');
    assertEquals('google', realbox.$.input.inputElement.value, 'input value');

    let start = realbox.$.input.inputElement.selectionStart!;
    let end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'gle', realbox.$.input.inputElement.value.substring(start, end));

    testProxy.handler.reset();

    // Tab key accepts the inline autocompletion,
    // moves the cursor to the end,
    // and re-queries the autocomplete with the full text.
    const tabEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    });
    realbox.$.inputWrapper.dispatchEvent(tabEvent);

    assertTrue(tabEvent.defaultPrevented, 'default prevented');

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals('google', args.input);

    assertEquals('google', realbox.$.input.inputElement.value);
    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(start, end);
    assertEquals(realbox.$.input.inputElement.value.length, start);

    // Shift+Tab correctly moves the focus to the previous icon
    // without triggering the autocompletion.
    realbox.$.input.inputElement.value = 'goo';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await testProxy.handler.whenCalled('queryAutocomplete');
    testProxy.handler.reset();

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    assertTrue(await areMatchesShowing(), 'matches showing');
    assertEquals('google', realbox.$.input.inputElement.value, 'input value');

    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'gle', realbox.$.input.inputElement.value.substring(start, end));

    testProxy.handler.reset();

    const shiftTabEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
      shiftKey: true,
    });
    realbox.$.inputWrapper.dispatchEvent(shiftTabEvent);

    assertEquals('goo', realbox.$.input.inputElement.value);
    assertFalse(shiftTabEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(start, end);
    assertEquals('goo'.length, start);
  });
});
