// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {NtpSearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {ContextType} from 'chrome://resources/cr_components/composebox/common.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {assertStyle, MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

const SAMPLE_INPUT_STATE = new MockInputState({
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
  toolsSectionConfig: {header: ''},
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
  modelSectionConfig: {header: ''},
  allowedInputTypes:
      [InputType.kLensImage, InputType.kLensFile, InputType.kBrowserTab],
  maxTotalInputs: 10,
});

function createAndAppendRealbox(properties: Partial<NtpSearchboxElement> = {}):
    NtpSearchboxElement {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const realbox = document.createElement('ntp-searchbox');
  Object.assign(realbox, properties);
  document.body.appendChild(realbox);
  return realbox;
}

suite('NewTabPageRealboxTabsTest', () => {
  let realbox: NtpSearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;

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
        lastActive: {internalValue: BigInt(1)},
      },
      {
        tabId: 2,
        title: 'Sample Tab 2',
        url: 'https://example.com/2',
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
  let realbox: NtpSearchboxElement;
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
    window.open = () => null;
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
    assertEquals(event.detail.files.length, 1);
    assertEquals(event.detail.files[0].tabId, 1);
    assertEquals(event.detail.files[0].title, 'title');
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
    realbox.$.input.inputElement.dispatchEvent(pasteEvent);
    await microtasksFinished();
    const event = await whenOpenComposeBox;

    assertTrue(!!event);
    assertEquals(event.detail.files.length, 2);
    const file1 = event.detail.files[0];
    assertEquals('pasted.png', file1.file.name);
    assertEquals('image/png', file1.file.type);
    const file2 = event.detail.files[1];
    assertEquals('pasted.pdf', file2.file.name);
    assertEquals('application/pdf', file2.file.type);
    assertFalse((realbox.$.input as any).pastedInInput_);
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

    realbox.$.input.inputElement.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertFalse(pasteEvent.defaultPrevented);
    assertFalse(openComposeboxCalled);
    assertTrue((realbox.$.input as any).pastedInInput_);
  });

  test('useWebKitSearchboxIcons with compose button enabled', async () => {
    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      searchboxChromeRefreshTheming: false,
      colorSourceIsBaseline: false,
    });
    await microtasksFinished();

    const buttonsToTest = [
      {
        selector: '#voiceSearchButton',
        iconUrl:
            'url("chrome://resources/cr_components/searchbox/icons/mic.svg")',
      },
      {
        selector: '#lensSearchButton',
        iconUrl: 'url("chrome://resources/cr_components/searchbox/icons/' +
            'camera.svg")',
      },
    ];
    for (const {selector, iconUrl} of buttonsToTest) {
      const button = realbox.shadowRoot.querySelector<HTMLElement>(selector);
      assertTrue(!!button);
      assertStyle(button, '-webkit-mask-image', iconUrl);
      assertStyle(button, 'background-image', 'none');
    }
  });

  test('clicking composebox button emits an event.', async () => {
    const whenOpenComposeBox = eventToPromise('open-composebox', realbox);

    const composeButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#composeButton');
    assertTrue(!!composeButton);

    composeButton.dispatchEvent(new CustomEvent('compose-click', {
      detail: {
        button: 0,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      },
      bubbles: true,
      composed: true,
    }));

    await whenOpenComposeBox;

    const metricName = 'ContextualSearch.AiModeButtonClick.NtpRealbox';
    assertEquals(2, metrics.count(metricName));
    assertEquals(1, metrics.count(metricName, true));
  });

  test('clicking composebox button with text records user action', () => {
    const inputEl = realbox.shadowRoot.querySelector('#input');
    assertTrue(!!inputEl);
    inputEl.shadowRoot!.querySelector<HTMLInputElement>('#input')!.value =
        'hello';
    inputEl.shadowRoot!.querySelector('#input')!.dispatchEvent(
        new InputEvent('input'));

    const composeButton =
        realbox.shadowRoot.querySelector<HTMLElement>('#composeButton');
    assertTrue(!!composeButton);

    composeButton.dispatchEvent(new CustomEvent('compose-click', {
      detail: {
        button: 0,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      },
      bubbles: true,
      composed: true,
    }));

    const submitUserActionName =
        'ContextualSearch.UserAction.SubmitQueryV2.WithoutContext.NewTabPage';
    assertEquals(1, metrics.count(submitUserActionName));

    const submitHistogramName =
        'ContextualSearch.UserAction.SubmitQueryV2.NewTabPage';
    assertEquals(1, metrics.count(submitHistogramName, /*WithoutContext*/ 0));

    const buttonMetricName = 'ContextualSearch.AiModeButtonClick.NtpRealbox';
    assertEquals(2, metrics.count(buttonMetricName));
    assertEquals(1, metrics.count(buttonMetricName, true));

    assertEquals(1, testProxy.handler.getCallCount('notifySessionStarted'));
    assertEquals(1, testProxy.handler.getCallCount('submitQuery'));
    const args = testProxy.handler.getArgs('submitQuery')[0];
    assertEquals('hello', args.queryText);  // query
  });

  test('hovering on composebox button plays the animation.', async () => {
    const composeButton =
        realbox.shadowRoot.querySelector('cr-searchbox-compose-button');
    assertTrue(!!composeButton);

    await composeButton.updateComplete;

    const glowAnimationWrapper =
        composeButton.shadowRoot.querySelector<HTMLElement>(
            '#glowAnimationWrapper');
    assertTrue(!!glowAnimationWrapper);

    glowAnimationWrapper.classList.remove('play');
    assertFalse(glowAnimationWrapper.classList.contains('play'));

    glowAnimationWrapper.dispatchEvent(new MouseEvent('mouseenter'));
    await microtasksFinished();

    const gradient = glowAnimationWrapper.querySelector('.gradient');
    const mask = glowAnimationWrapper.querySelector('.mask');

    const gradientBeforeStyle = getComputedStyle(gradient!, '::before');
    const maskBeforeStyle = getComputedStyle(mask!, '::before');

    assertEquals('running', gradientBeforeStyle.animationPlayState);
    assertEquals('running', maskBeforeStyle.animationPlayState);
  });

  test('composeanimation does not play on page load.', async () => {
    loadTimeData.overrideValues({searchboxShowComposeAnimation: true});

    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
      searchboxLayoutMode: 'Compact',
    });
    await microtasksFinished();

    const composeButton =
        realbox.shadowRoot.querySelector('cr-searchbox-compose-button');
    assertTrue(!!composeButton);
    await composeButton.updateComplete;

    const glowAnimationWrapper =
        composeButton.shadowRoot.querySelector<HTMLElement>(
            '#glowAnimationWrapper');
    assertTrue(!!glowAnimationWrapper);
    assertFalse(glowAnimationWrapper.classList.contains('play'));
  });

  test('compose animation does not play on page load.', async () => {
    loadTimeData.overrideValues({searchboxShowComposeAnimation: false});

    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
      searchboxLayoutMode: 'Compact',
    });
    await microtasksFinished();

    const composeButton =
        realbox.shadowRoot.querySelector('cr-searchbox-compose-button');
    assertTrue(!!composeButton);
    await composeButton.updateComplete;

    const glowAnimationWrapper =
        composeButton.shadowRoot.querySelector<HTMLElement>(
            '#glowAnimationWrapper');
    assertTrue(!!glowAnimationWrapper);
    assertFalse(glowAnimationWrapper.classList.contains('play'));
  });

  test('tabbing with inline autocompletion', async () => {
    realbox.$.input.focus();
    assertEquals(realbox.$.input, realbox.shadowRoot.activeElement);

    realbox.$.input.inputElement.value = 'goo';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'gle',
    })];

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    await microtasksFinished();
    assertEquals('google', realbox.$.input.inputElement.value, 'input value');

    let start = realbox.$.input.inputElement.selectionStart!;
    let end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'gle', realbox.$.input.inputElement.value.substring(start, end));

    // Tab key accepts the inline autocompletion, moves the cursor to the end,
    // and re-queries the autocomplete with the full text.
    const tabEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    });
    realbox.$.inputWrapper.dispatchEvent(tabEvent);
    assertTrue(tabEvent.defaultPrevented, 'default prevented');

    assertEquals('google', realbox.$.input.inputElement.value);
    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(start, end);
    assertEquals(realbox.$.input.inputElement.value.length, start);

    // Shift+Tab clears inline autocompletion without triggering a new query.
    realbox.$.input.inputElement.value = 'goo';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    await microtasksFinished();
    assertEquals('google', realbox.$.input.inputElement.value, 'input value');

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

    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(start, end);
    assertEquals('goo'.length, start);
  });

  test('metrics are recorded for ToolMode clicks', async () => {
    loadTimeData.overrideValues({
      composeboxSource: 'TestSource',
    });
    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
    });
    await microtasksFinished();

    const entrypointAndMenu = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);

    entrypointAndMenu.dispatchEvent(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kDeepSearch},
    }));

    const metricName =
        'TestSource.AimEntrypoint.ClassicPopup.ContextualElement.Clicked';
    assertEquals(1, metrics.count(metricName, ContextType.DEEP_RESEARCH));

    entrypointAndMenu.dispatchEvent(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kImageGen},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE_GEN));

    entrypointAndMenu.dispatchEvent(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kCanvas},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.CANVAS));
  });

  test('metrics are recorded for ModelMode clicks', async () => {
    loadTimeData.overrideValues({
      composeboxSource: 'TestSource',
    });
    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
    });
    await microtasksFinished();

    const entrypointAndMenu = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);

    const metricName =
        'TestSource.AimEntrypoint.ClassicPopup.ContextualElement.Clicked';

    entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiProAutoroute},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.AUTO_MODEL));

    entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiPro},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.THINKING_MODEL));

    entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiRegular},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.REGULAR_MODEL));

    entrypointAndMenu.dispatchEvent(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiProNoGenUi},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.PRO_NO_GEN_UI_MODEL));
  });

  test('metrics are recorded for file uploads', async () => {
    loadTimeData.overrideValues({
      composeboxSource: 'TestSource',
    });
    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
    });
    await microtasksFinished();

    const fileInputs =
        realbox.shadowRoot.querySelector('cr-composebox-file-inputs');
    assertTrue(!!fileInputs);

    const metricName =
        'TestSource.AimEntrypoint.ClassicPopup.ContextualElement.Clicked';

    const dataTransferImage = new DataTransfer();
    dataTransferImage.items.add(
        new File([''], 'test.png', {type: 'image/png'}));

    fileInputs.dispatchEvent(new CustomEvent('file-change', {
      detail: {files: dataTransferImage.files},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE));

    const dataTransferFile = new DataTransfer();
    dataTransferFile.items.add(
        new File([''], 'test.pdf', {type: 'application/pdf'}));

    fileInputs.dispatchEvent(new CustomEvent('file-change', {
      detail: {files: dataTransferFile.files},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.FILE));
  });

  test('metrics are recorded for tab additions', async () => {
    loadTimeData.overrideValues({
      composeboxSource: 'TestSource',
    });
    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
    });
    await microtasksFinished();

    const entrypointAndMenu = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);

    const metricName =
        'TestSource.AimEntrypoint.ClassicPopup.ContextualElement.Clicked';

    entrypointAndMenu.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {
        id: 1,
        title: 'Title',
        url: {url: 'http://test.com'},
        delayUpload: false,
        origin: 0,
      },
    }));
    assertEquals(1, metrics.count(metricName, ContextType.TAB));
  });

  test('metrics are recorded for shown context menu items', async () => {
    loadTimeData.overrideValues({
      composeboxSource: 'TestSource',
    });
    realbox = createAndAppendRealbox({
      composeButtonEnabled: true,
      composeboxEnabled: true,
      ntpRealboxNextEnabled: true,
    });
    await microtasksFinished();

    const metricName =
        'TestSource.AimEntrypoint.ClassicPopup.ContextualElement.Shown';

    // Verify no metrics are recorded initially.
    assertEquals(0, metrics.count(metricName));

    // Setup tab suggestions.
    const sampleTabs = [
      {
        tabId: 1,
        title: 'Sample Tab 1',
        url: 'https://example.com/1',
        lastActive: {internalValue: BigInt(1)},
      },
      {
        tabId: 2,
        title: 'Sample Tab 2',
        url: 'https://example.com/2',
        lastActive: {internalValue: BigInt(2)},
      },
    ];
    testProxy.handler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

    // Open context menu to trigger shown metrics.
    const entrypointAndMenu = realbox.shadowRoot.querySelector(
        'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);
    entrypointAndMenu.dispatchEvent(new CustomEvent('context-menu-opened'));
    await microtasksFinished();

    assertEquals(testProxy.handler.getCallCount('getRecentTabs'), 1);
    assertDeepEquals((realbox as any).tabSuggestions_, sampleTabs);

    // Verify shown metrics for input types based on SAMPLE_INPUT_STATE
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE));
    assertEquals(1, metrics.count(metricName, ContextType.FILE));
    assertEquals(1, metrics.count(metricName, ContextType.TAB));

    // Verify shown metrics for tools based on SAMPLE_INPUT_STATE
    assertEquals(1, metrics.count(metricName, ContextType.DEEP_RESEARCH));
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE_GEN));
    assertEquals(0, metrics.count(metricName, ContextType.CANVAS));

    // Verify shown metrics for models based on SAMPLE_INPUT_STATE
    assertEquals(1, metrics.count(metricName, ContextType.REGULAR_MODEL));
    assertEquals(1, metrics.count(metricName, ContextType.THINKING_MODEL));
    assertEquals(0, metrics.count(metricName, ContextType.AUTO_MODEL));
  });

  test(
      'clicking composebox button with a URL clears text and opens composebox',
      async () => {
        const inputEl = realbox.shadowRoot.querySelector('#input');
        assertTrue(!!inputEl);
        inputEl.shadowRoot!.querySelector<HTMLInputElement>('#input')!.value =
            'https://example.com';
        inputEl.shadowRoot!.querySelector('#input')!.dispatchEvent(
            new InputEvent('input'));

        const matches = [createSearchMatchForTesting({
          allowedToBeDefaultMatch: true,
          isSearchType: false,
        })];

        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: 'https://example.com',
              matches: matches,
            }));
        await microtasksFinished();

        const composeButton =
            realbox.shadowRoot.querySelector<HTMLElement>('#composeButton');
        assertTrue(!!composeButton);

        const whenOpenComposeBox =
            eventToPromise('open-composebox', realbox);

        composeButton.dispatchEvent(new CustomEvent('compose-click', {
          detail: {
            button: 0,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
          bubbles: true,
          composed: true,
        }));

        const event = await whenOpenComposeBox;
        assertEquals('', event.detail.text);
        assertEquals('', realbox.$.input.inputElement.value);
      });

  test(
      'composebox button with URL and composebox disabled sends empty query',
      async () => {
        realbox.composeboxEnabled = false;

        const inputEl = realbox.shadowRoot.querySelector('#input');
        assertTrue(!!inputEl);
        inputEl.shadowRoot!.querySelector<HTMLInputElement>('#input')!.value =
            'https://example.com';
        inputEl.shadowRoot!.querySelector('#input')!.dispatchEvent(
            new InputEvent('input'));

        const matches = [createSearchMatchForTesting({
          allowedToBeDefaultMatch: true,
          isSearchType: false,
        })];

        testProxy.callbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: 'https://example.com',
              matches: matches,
            }));
        await microtasksFinished();

        const composeButton =
            realbox.shadowRoot.querySelector<HTMLElement>('#composeButton');
        assertTrue(!!composeButton);

        composeButton.dispatchEvent(new CustomEvent('compose-click', {
          detail: {
            button: 0,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
          bubbles: true,
          composed: true,
        }));

        assertEquals(1, testProxy.handler.getCallCount('submitQuery'));
        const args = testProxy.handler.getArgs('submitQuery')[0];
        assertEquals('', args.queryText);
      });
});
