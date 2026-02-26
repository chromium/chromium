// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {SearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

function createAndAppendRealbox(properties: Partial<SearchboxElement> = {}):
    SearchboxElement {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const realbox = document.createElement('cr-searchbox');
  Object.assign(realbox, properties);
  document.body.appendChild(realbox);
  return realbox;
}

suite('NewTabPageRealboxTabsTest', () => {
  let realbox: SearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    loadTimeData.overrideValues({
      ntpRealboxNextEnabled: true,
    });

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    realbox = await createAndAppendRealbox(
        {ntpRealboxNextEnabled: true, searchboxLayoutMode: 'Compact'});
  });

  test('on tab strip change does not trigger getRecentTabs call', async () => {
    testProxy.callbackRouterRemote.onTabStripChanged();
    await microtasksFinished();

    // Tab strip change does not trigger getRecentTabs call automatically.
    assertEquals(testProxy.handler.getCallCount('getRecentTabs'), 0);
  });

  test('getRecentTabs only fires when context menu is open', async () => {
    const contextElement = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!contextElement);
    contextElement.dispatchEvent(new CustomEvent('context-menu-opened'));
    await microtasksFinished();

    // A forced getRecentTabs call is made when the context menu is opened.
    assertEquals(testProxy.handler.getCallCount('getRecentTabs'), 1);
    testProxy.handler.reset();

    const sampleTabs = [
      {
        tabId: 1,
        title: 'Sample Tab 1',
        url: 'https://example.com/1',
        showInRecentTabChip: true,
        lastActive: {internalValue: BigInt(1)},
      },
      {
        tabId: 2,
        title: 'Sample Tab 2',
        url: 'https://example.com/2',
        showInRecentTabChip: true,
        lastActive: {internalValue: BigInt(2)},
      },
    ];
    testProxy.handler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

    testProxy.callbackRouterRemote.onTabStripChanged();
    await microtasksFinished();

    assertEquals(testProxy.handler.getCallCount('getRecentTabs'), 1);
    assertDeepEquals((realbox as any).tabSuggestions_, sampleTabs);

    // Once the context menu is closed again, getRecentTabs should not be called
    // on tab strip changes.
    contextElement.dispatchEvent(new CustomEvent('context-menu-closed'));
    await microtasksFinished();
    testProxy.handler.reset();

    testProxy.callbackRouterRemote.onTabStripChanged();
    await microtasksFinished();
    assertEquals(testProxy.handler.getCallCount('getRecentTabs'), 0);
  });
});

suite('NewTabPageRealboxNextTest', () => {
  let realbox: SearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let metrics: MetricsTracker;

  setup(() => {
    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
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
    const testMetricsReporterProxy = TestMock.fromClass(BrowserProxyImpl);
    testMetricsReporterProxy.reset();
    const metricsReporterCallbackRouter = new PageMetricsCallbackRouter();
    testMetricsReporterProxy.setResultFor(
        'getCallbackRouter', metricsReporterCallbackRouter);
    testMetricsReporterProxy.setResultFor('getMark', Promise.resolve(null));
    BrowserProxyImpl.setInstance(testMetricsReporterProxy);
    MetricsReporterImpl.setInstanceForTest(new MetricsReporterImpl());
    metrics = fakeMetricsPrivate();

    testProxy.handler.setResultFor('getInputState', {
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        inputTypeConfigs: [],
        toolConfigs: [],
        modelConfigs: [],
        toolsSectionConfig: null,
        modelSectionConfig: null,
        hintText: '',
        maxInstances: {},
        maxTotalInputs: 0,
      },
    });
    realbox = createAndAppendRealbox();
  });

  test('adding context files opens composebox', async () => {
    // Arrange.
    realbox = await createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      searchboxLayoutMode: 'TallBottomContext',
      ntpRealboxNextEnabled: true,
    });
    const contextElement = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!contextElement);

    // Act & Assert.
    const whenOpenComposeBox = eventToPromise('open-composebox', realbox);
    contextElement.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {id: 1, title: 'title'},
      bubbles: true,
      composed: true,
    }));
    const event = await whenOpenComposeBox;
    assertEquals(event.detail.contextFiles.length, 1);
    assertEquals(event.detail.contextFiles[0].tabId, 1);
    assertEquals(event.detail.contextFiles[0].title, 'title');
  });

  test('clicking deep search button opens composebox', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      composeboxShowDeepSearchButton: true,
    });
    realbox = await createAndAppendRealbox(
        {ntpRealboxNextEnabled: true, searchboxLayoutMode: 'Compact'});
    const contextElement = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!contextElement);
    const contextMenuEntrypoint = contextElement.shadowRoot.querySelector(
        'cr-composebox-context-menu-entrypoint');
    assertTrue(!!contextMenuEntrypoint);

    testProxy.handler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));

    // Act.
    const whenOpenComposeBox = eventToPromise('open-composebox', realbox);

    const entrypointButton =
        contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
            '#entrypoint');
    assertTrue(entrypointButton !== null);
    entrypointButton.click();
    await microtasksFinished();

    const deepSearchButton =
        contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
            '#deepSearch');
    assertTrue(!!deepSearchButton);
    deepSearchButton.click();

    // Assert.
    const event = await whenOpenComposeBox;
    assertEquals(ToolMode.kDeepSearch, event.detail.mode);
    // Calling deep search should not be logged as context being added.
    assertEquals(
        0,
        metrics.count(
            'ContextualSearch.ContextAdded.ContextAddedMethod.NewTabPage'));
  });

  test('clicking create image button opens composebox', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      composeboxShowCreateImageButton: true,
    });
    realbox = await createAndAppendRealbox(
        {ntpRealboxNextEnabled: true, searchboxLayoutMode: 'Compact'});
    const contextElement = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!contextElement);
    const contextMenuEntrypoint = contextElement.shadowRoot.querySelector(
        'cr-composebox-context-menu-entrypoint');
    assertTrue(!!contextMenuEntrypoint);

    testProxy.handler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));

    // Act.
    const whenOpenComposeBox = eventToPromise('open-composebox', realbox);

    const entrypointButton =
        contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
            '#entrypoint');
    assertTrue(entrypointButton !== null);
    entrypointButton.click();
    await microtasksFinished();

    const createImageButton =
        contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
            '#createImage');
    assertTrue(!!createImageButton);
    createImageButton.click();

    // Assert.
    const event = await whenOpenComposeBox;
    assertEquals(ToolMode.kImageGen, event.detail.mode);
  });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip(
      'Contextual component empty area click focuses search input',
      async () => {
        // Arrange.
        realbox = await createAndAppendRealbox({
          composeButtonEnabled: true,
          composeboxEnabled: true,
          searchboxLayoutMode: 'TallTopContext',
          ntpRealboxNextEnabled: true,
        });
        const contextElement = realbox.shadowRoot.querySelector(
            'cr-composebox-contextual-entrypoint-and-menu');
        assertTrue(!!contextElement);
        contextElement.dispatchEvent(
            new CustomEvent('context-menu-container-click'));
        assertEquals(1, testProxy.handler.getCallCount('onFocusChanged'));
        assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
      });

  test('pasting files opens composebox', async () => {
    loadTimeData.overrideValues({composeboxFileMaxCount: 2});
    realbox = await createAndAppendRealbox({ntpRealboxNextEnabled: true});

    const pngFile = new File([''], 'pasted.png', {type: 'image/png'});
    const pdfFile = new File([''], 'pasted.pdf', {type: 'application/pdf'});

    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(pngFile);
    dataTransfer.items.add(pdfFile);
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    const whenOpenComposeBox = eventToPromise('open-composebox', realbox);
    realbox.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();
    const event = await whenOpenComposeBox;

    assertTrue(!!event);
    assertEquals(event.detail.contextFiles.length, 2);
    const file1 = event.detail.contextFiles[0];
    assertEquals('pasted.png', file1.file.name);
    assertEquals('image/png', file1.file.type);
    const file2 = event.detail.contextFiles[1];
    assertEquals('pasted.pdf', file2.file.name);
    assertEquals('application/pdf', file2.file.type);
    assertFalse((realbox as any).pastedInInput_);
  });

  test('pasting text sets pastedInInput flag', async () => {
    // Re-create realbox to pick up new loadTimeData.
    realbox = await createAndAppendRealbox({ntpRealboxNextEnabled: true});

    let openComposeboxCalled = false;
    realbox.addEventListener('open-composebox', () => {
      openComposeboxCalled = true;
    });

    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/plain', 'hello');
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    realbox.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertFalse(pasteEvent.defaultPrevented);
    assertFalse(openComposeboxCalled);
    assertTrue((realbox as any).pastedInInput_);
  });
});
