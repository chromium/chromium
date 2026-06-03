// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {NtpSearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

// This is the realbox's lens button tests, not the lens searchbox's tests.
suite('Lens search in ntp realbox', () => {
  let realbox: NtpSearchboxElement;

  let testProxy: TestSearchboxBrowserProxy;

  const testMetricsReporterProxy = TestMock.fromClass(BrowserProxyImpl);

  async function areMatchesShowing(): Promise<boolean> {
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    return window.getComputedStyle(realbox.getDropdownElement()).display !==
        'none';
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      searchboxLensSearch: true,
      realboxMatchOmniboxTheme: true,
      searchboxSeparator: ' - ',
    });
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

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

    realbox = document.createElement('ntp-searchbox');
    document.body.appendChild(realbox);
  });

  test('Lens search button is present by default', async () => {
    // Arrange.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('ntp-searchbox');
    document.body.appendChild(realbox);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert
    const lensButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
    assertTrue(!!lensButton);
  });

  test('Lens search button is not present when not enabled', async () => {
    // Arrange.
    loadTimeData.overrideValues({searchboxLensSearch: false});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('ntp-searchbox');
    document.body.appendChild(realbox);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert
    const lensButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
    assertFalse(!!lensButton);

    // Restore
    loadTimeData.overrideValues({searchboxLensSearch: true});
  });

  test('clicking Lens search button hides matches', async () => {
    // Arrange.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('ntp-searchbox');
    document.body.appendChild(realbox);

    // Act.
    realbox.$.input.inputElement.value = '';
    realbox.$.input.inputElement.dispatchEvent(
        new MouseEvent('mousedown', {button: 0}));

    const matches = [createAutocompleteMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches,
        }));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(await areMatchesShowing());

    // Act.
    const lensButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
    assertTrue(!!lensButton);
    lensButton.click();

    // Assert.
    assertFalse(await areMatchesShowing());
  });

  test('clicking Lens search button sends Lens search event', async () => {
    // Arrange.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('ntp-searchbox');
    document.body.appendChild(realbox);
    const whenOpenLensSearch = eventToPromise('open-lens-search', realbox);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Act.
    const lensButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#lensSearchButton');
    assertTrue(!!lensButton);
    lensButton.click();

    // Assert.
    await whenOpenLensSearch;
  });
});
