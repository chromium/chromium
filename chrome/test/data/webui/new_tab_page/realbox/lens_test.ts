// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/new_tab_page.js';

import {BrowserProxyImpl, MetricsReporterImpl, mojoString16, RealboxBrowserProxy, RealboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {AutocompleteMatch} from 'chrome://resources/cr_components/omnibox/omnibox.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter/metrics_reporter.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestRealboxBrowserProxy} from './test_realbox_browser_proxy.js';

function createAutocompleteMatch(): AutocompleteMatch {
  return {
    a11yLabel: mojoString16(''),
    allowedToBeDefaultMatch: false,
    isSearchType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,  // Indicates a missing suggestion group Id.
    contents: mojoString16(''),
    contentsClass: [{offset: 0, style: 0}],
    description: mojoString16(''),
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: {url: ''},
    inlineAutocompletion: mojoString16(''),
    fillIntoEdit: mojoString16(''),
    iconUrl: '',
    imageDominantColor: '',
    imageUrl: '',
    removeButtonA11yLabel: mojoString16(''),
    type: '',
    isRichSuggestion: false,
  };
}

suite('Lens search', () => {
  let realbox: RealboxElement;

  let testProxy: TestRealboxBrowserProxy;

  const testMetricsReporterProxy = TestBrowserProxy.fromClass(BrowserProxyImpl);

  function areMatchesShowing(): boolean {
    // Force a synchronous render.
    [...realbox.$.matches.shadowRoot!.querySelectorAll('dom-repeat')].forEach(
        template => template.render());
    return window.getComputedStyle(realbox.$.matches).display !== 'none';
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      realboxMatchOmniboxTheme: true,
      realboxSeparator: ' - ',
    });
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Set up Realbox's browser proxy.
    testProxy = new TestRealboxBrowserProxy();
    RealboxBrowserProxy.setInstance(testProxy);

    // Set up MetricsReporter's browser proxy.
    testMetricsReporterProxy.reset();
    const metricsReporterCallbackRouter = new PageMetricsCallbackRouter();
    testMetricsReporterProxy.setResultFor(
        'getCallbackRouter', metricsReporterCallbackRouter);
    testMetricsReporterProxy.setResultFor('getMark', Promise.resolve(null));
    BrowserProxyImpl.setInstance(testMetricsReporterProxy);
    MetricsReporterImpl.setInstanceForTest(new MetricsReporterImpl());

    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);
  });

  test('Lens search button is visible when feature is flipped', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      realboxLensSearch: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert
    const lensButton =
        realbox.shadowRoot!.querySelector('#lensSearchButton') as HTMLElement;
    assertTrue(!!lensButton);
  });

  test('clicking Lens search button hides matches', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      realboxLensSearch: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);

    // Act.
    realbox.$.input.value = 'hello';
    realbox.$.input.dispatchEvent(new InputEvent('input'));

    const matches = [createAutocompleteMatch()];
    testProxy.callbackRouterRemote.autocompleteResultChanged({
      input: mojoString16(realbox.$.input.value.trimStart()),
      matches,
      suggestionGroupsMap: {},
    });
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(areMatchesShowing());

    // Act.
    const lensButton =
        realbox.shadowRoot!.querySelector('#lensSearchButton') as HTMLElement;
    lensButton.click();

    // Assert.
    assertFalse(areMatchesShowing());

    // Restore.
    loadTimeData.overrideValues({
      realboxLensSearch: false,
    });
  });

  test('clicking Lens search button sends Lens search event', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      realboxLensSearch: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    realbox = document.createElement('ntp-realbox');
    document.body.appendChild(realbox);
    const whenOpenLensSearch = eventToPromise('open-lens-search', realbox);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Act.
    const lensButton =
        realbox.shadowRoot!.querySelector('#lensSearchButton') as HTMLElement;
    lensButton.click();

    // Assert.
    await whenOpenLensSearch;

    // Restore.
    loadTimeData.overrideValues({
      realboxLensSearch: false,
    });
  });
});
