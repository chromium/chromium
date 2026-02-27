// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {SearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

const SAMPLE_INPUT_STATE = createInputState({
  allowedTools: [ToolMode.kDeepSearch, ToolMode.kImageGen],
  toolConfigs: [
    {
      tool: ToolMode.kDeepSearch,
      menuLabel: 'Deep Search',
      disableActiveModelSelection: false,
      chipLabel: '',
      hintText: '',
      aimUrlParams: [],
    },
    {
      tool: ToolMode.kImageGen,
      menuLabel: 'Generate Image',
      disableActiveModelSelection: false,
      chipLabel: '',
      hintText: '',
      aimUrlParams: [],
    },
  ],
  allowedModels: [ModelMode.kGeminiRegular, ModelMode.kGeminiPro],
  modelConfigs: [
    {
      model: ModelMode.kGeminiRegular,
      menuLabel: 'Gemini Regular',
      hintText: '',
      aimUrlParams: [],
    },
    {
      model: ModelMode.kGeminiPro,
      menuLabel: 'Gemini Pro',
      hintText: '',
      aimUrlParams: [],
    },
  ],
  allowedInputTypes:
      [InputType.kLensImage, InputType.kLensFile, InputType.kBrowserTab],
  maxTotalInputs: 10,
});

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

  suiteSetup(() => {
    loadTimeData.overrideValues({
      composeboxShowRecentTabChip: true,
      contextualMenuUsePecApi: true,
      isLensSearchbox: false,
      reportMetrics: true,
      searchboxCyclingPlaceholders: false,
      searchboxDefaultIcon: 'search.svg',
      searchboxSeparator: ' - ',
      searchboxVoiceSearch: true,
    });
  });

  setup(() => {
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    realbox = createAndAppendRealbox(
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

  test('recent tab chip visibility depends on allowed input types', async () => {
    const sampleTabs = [
      {
        tabId: 1,
        title: 'Sample Tab 1',
        url: 'https://example.com/1',
        showInCurrentTabChip: true,
        lastActive: {internalValue: BigInt(1)},
      },
    ];
    testProxy.handler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

    // Case 1: Browser tab allowed
    testProxy.handler.setResultFor('getInputState', {
      state: createInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      }),
    });

    realbox = await createAndAppendRealbox(
        {ntpRealboxNextEnabled: true, searchboxLayoutMode: 'Compact'});

    realbox.$.input.focus();
    await microtasksFinished();
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    await testProxy.handler.whenCalled('getRecentTabs');

    // Show dropdown (required for chip visibility)
    const matches = [createSearchMatchForTesting()];
    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    let chipContainer =
        realbox.shadowRoot.querySelector('#recentTabChipContainer');
    assertTrue(!!chipContainer);

    // Case 2: Browser tab NOT allowed
    testProxy.handler.reset();
    testProxy.handler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: sampleTabs}));
    testProxy.handler.setResultFor('getInputState', {
      state: createInputState({
        allowedInputTypes: [],  // No browser tab
      }),
    });

    realbox = await createAndAppendRealbox(
        {ntpRealboxNextEnabled: true, searchboxLayoutMode: 'Compact'});

    realbox.$.input.focus();
    await microtasksFinished();
    realbox.$.input.dispatchEvent(new MouseEvent('mousedown', {button: 0}));
    await testProxy.handler.whenCalled('getRecentTabs');

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    chipContainer =
        realbox.shadowRoot.querySelector('#recentTabChipContainer');
    assertFalse(!!chipContainer);
  });
});

suite('NewTabPageRealboxNextTest', () => {
  let realbox: SearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let metrics: MetricsTracker;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      contextualMenuUsePecApi: true,
      isLensSearchbox: false,
      reportMetrics: true,
      searchboxCyclingPlaceholders: false,
      searchboxDefaultIcon: 'search.svg',
      searchboxSeparator: ' - ',
      searchboxVoiceSearch: true,
    });
  });

  setup(async () => {
    // Set up Realbox's browser proxy.
    testProxy = new TestSearchboxBrowserProxy();
    testProxy.handler.setResultFor(
        'getInputState', Promise.resolve({state: SAMPLE_INPUT_STATE}));
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
    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
      searchboxLayoutMode: 'Compact',
    });
    await microtasksFinished();
  });

  test('adding context files opens composebox', async () => {
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
    const entrypointAndMenu = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu, 'contextual-entrypoint-and-menu');
    const contextMenuEntrypoint = entrypointAndMenu.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-button');
    assertTrue(!!contextMenuEntrypoint, 'contextual entrypoint button');

    // Act.
    const whenOpenComposeBox = eventToPromise('open-composebox', realbox);

    const entrypointButton =
        contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
            '#entrypoint');
    assertTrue(!!entrypointButton, 'Entrypoint button');
    entrypointButton.click();
    await microtasksFinished();

    const actionMenu = entrypointAndMenu.shadowRoot.querySelector(
        'cr-composebox-contextual-action-menu');
    assertTrue(!!actionMenu, 'Action menu');
    const deepSearchButton = actionMenu.shadowRoot.querySelector<HTMLElement>(
        `button[data-mode="${ToolMode.kDeepSearch}"]`);
    assertTrue(!!deepSearchButton, 'Deep search button');
    deepSearchButton.click();
    await microtasksFinished();

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
    const entrypointAndMenu = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu, 'contextual-entrypoint-and-menu');
    const contextMenuEntrypoint = entrypointAndMenu.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-button');
    assertTrue(!!contextMenuEntrypoint, 'contextual-entrypoint-button');

    // Act.
    const whenOpenComposeBox = eventToPromise('open-composebox', realbox);

    const entrypointButton =
        contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
            '#entrypoint');
    assertTrue(!!entrypointButton, 'Entrypoint button');
    entrypointButton.click();
    await microtasksFinished();

    const actionMenu = entrypointAndMenu.shadowRoot.querySelector(
        'cr-composebox-contextual-action-menu');
    assertTrue(!!actionMenu, 'Action menu');
    const createImageButton = actionMenu.shadowRoot.querySelector<HTMLElement>(
        `button[data-mode="${ToolMode.kImageGen}"]`);
    assertTrue(!!createImageButton, 'Create images button');
    createImageButton.click();
    await microtasksFinished();

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
            'contextual-entrypoint-and-carousel');
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
