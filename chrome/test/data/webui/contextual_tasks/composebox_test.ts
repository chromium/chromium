// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxToolChipElement} from 'chrome://resources/cr_components/composebox/composebox_tool_chip.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteResult, PageRemote as SearchboxPageRemote, SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createCtComposeboxApp, fixtureUrl, getSubmitButton, simulateUserInput} from './contextual_tasks_test_utils.js';
import type {CtComposeboxAppParts} from './contextual_tasks_test_utils.js';
import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {setupAutocompleteResults} from './test_searchbox_utils.js';

declare global {
  interface Window {
    chrome: {
      histograms?: {
        recordEnumerationValue: (name: string, value: number, max: number) =>
            void,
        recordUserAction: (action: string) => void,
        recordBoolean: (name: string, value: boolean) => void,
      },
    };
  }
}

function pressEnter(element: HTMLElement) {
  element.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Enter',
    bubbles: true,
    composed: true,
  }));
}

suite('ContextualTasksComposeboxTest', () => {
  let contextualTasksApp: ContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
      ComposeboxPageHandlerRemote;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
      SearchboxPageHandlerRemote;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let mockTimer: MockTimer;

  async function setActiveTool(tool: ToolMode) {
    searchboxCallbackRouterRemote.onInputStateChanged({
      ...new MockInputState(),
      activeTool: tool,
      toolConfigs: tool === ToolMode.kCanvas ? [{
        tool: ToolMode.kCanvas,
        disableActiveModelSelection: false,
        menuLabel: 'Canvas',
        chipLabel: 'Canvas',
        hintText: 'Canvas hint',
        aimUrlParams: [{paramKey: 'rc', paramValue: '1'}],
        menuTooltip: '',
      }] :
                                               [],
    });
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
  }
  class MockResizeObserver {
    static instances: MockResizeObserver[] = [];

    constructor(private callback: ResizeObserverCallback) {
      MockResizeObserver.instances.push(this);
    }

    observe(_target: Element) {}
    unobserve(_target: Element) {}
    disconnect() {}

    trigger() {
      // Trigger with empty entries as the component doesn't use entries
      this.callback([], this);
    }
  }

  setup(async () => {
    if (!window.chrome) {
      Object.assign(window, {chrome: {}});
    }

    if (!window.chrome.histograms) {
      Object.assign(window.chrome, {
        histograms: {
          recordEnumerationValue: () => {},
          recordUserAction: () => {},
          recordBoolean: () => {},
        },
      });
    }
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Mock ResizeObserver
    window.ResizeObserver = MockResizeObserver;
    MockResizeObserver.instances = [];

    mockTimer = new MockTimer();

    loadTimeData.overrideValues({
      useContextualTasksComposeboxFork: false,
      contextualMenuUsePecApi: false,
      composeboxSmartTabSharingVisible: false,
      enableComposeboxJumpFix: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
      composeboxHintTextLensOverlay: 'Test Lens Hint',
      composeboxHintTextAskAboutThese: 'Ask about these',
      composeboxHintTextAskAboutThisTab: 'Ask about this tab',
      composeboxHintTextAskAboutThisImage: 'Ask about this image',
      composeboxHintTextAskAboutThisDoc: 'Ask about this doc',
      forcedEmbeddedPageHost: '',
      tabFaviconChipsToCoinsEnabled: false,
    });

    testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    mockComposeboxPageHandler.setResultFor(
        'canShowNextboxAnimation', Promise.resolve({canShow: true}));
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    mockSearchboxPageHandler.setResultFor(
        'addTabContext', Promise.resolve({high: BigInt(1), low: BigInt(2)}));
    mockSearchboxPageHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        toolConfigs: [{
          tool: ToolMode.kCanvas,
          disableActiveModelSelection: false,
          menuLabel: 'Canvas',
          chipLabel: 'Canvas',
          hintText: 'Canvas hint',
          aimUrlParams: [{paramKey: 'rc', paramValue: '1'}],
          menuTooltip: '',
        }],
      },
    }));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);
    await microtasksFinished();
    composebox = contextualTasksApp.$.composebox.$.composebox;

    assertTrue(
        MockResizeObserver.instances.length >= 1,
        'There should be at least one ResizeObserver instance.');

    searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState());
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  test('FocusUpdatesProperty', () => {
    mockTimer.install();
    const composebox = contextualTasksApp.$.composebox;
    const innerComposebox = composebox.$.composebox;

    innerComposebox.dispatchEvent(new CustomEvent('composebox-focus-in'));
    mockTimer.tick(0);  // Attribute reflection is async
    assertTrue(composebox.isComposeboxFocusedForTesting);

    innerComposebox.dispatchEvent(new CustomEvent('composebox-focus-out'));
    assertTrue(!composebox.isComposeboxFocusedForTesting);
  });

  test('ResizeUpdatesHeight', () => {
    mockTimer.install();
    const composebox = contextualTasksApp.$.composebox;
    const innerComposebox = composebox.$.composebox;


    innerComposebox.style.display = 'block';
    innerComposebox.style.height = '100px';


    Object.defineProperty(innerComposebox, 'offsetHeight', {
      writable: true,
      configurable: true,
      value: 100,
    });

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    const height1 = composebox.composeboxHeightForTesting;
    assertTrue(typeof height1 === 'number');
    assertTrue(height1 > 0, `height1 should be > 0, but is ${height1}`);

    innerComposebox.style.height = '300px';

    // Update mock
    Object.defineProperty(innerComposebox, 'offsetHeight', {
      writable: true,
      configurable: true,
      value: 300,
    });

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    const height2 = composebox.composeboxHeightForTesting;
    assertTrue(typeof height2 === 'number');
    assertTrue(
        height1 !== height2, `Height should change: ${height1} vs ${height2}`);
  });

  // TODO(crbug.com/523350742): Enable Tooltip tests on Android.
  // <if expr="not is_android">
  test('TooltipVisibilityUpdatesOnResize', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = contextualTasksApp.$.onboardingTooltip;

    // Force show tooltip
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
    });
    contextualTasksApp.numberOfTimesTooltipShownForTesting = 0;
    contextualTasksApp.userDismissedTooltipForTesting = false;

    // Simulate active tab chip token presence
    const innerComposebox = composeboxElement.$.composebox;
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    contextualTasksApp.updateTooltipVisibilityForTesting();
    assertTrue(tooltip!.shouldShow);

    // Resize event
    const resizeEvent = new CustomEvent('composebox-resize', {
      detail: {carouselHeight: 50},
      bubbles: true,
      composed: true,
    });
    innerComposebox.dispatchEvent(resizeEvent);

    // Tooltip should still be shown and position updated (implicitly via resize
    // observer or logic)
    assertTrue(tooltip!.shouldShow);
  });

  test('TooltipResizeObserverCoexistsWithResizeObserver', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const innerComposebox = composeboxElement.$.composebox;

    // Initially, only resizeObserver_ should exist.
    assertTrue(composeboxElement.resizeObserverForTesting !== null);
    assertFalse(contextualTasksApp.tooltipResizeObserverForTesting !== null);

    // Force show tooltip.
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
    });
    contextualTasksApp.numberOfTimesTooltipShownForTesting = 0;
    contextualTasksApp.userDismissedTooltipForTesting = false;

    // Simulate active tab chip token presence to trigger tooltip.
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    contextualTasksApp.updateTooltipVisibilityForTesting();

    // Now both observers should exist.
    assertTrue(composeboxElement.resizeObserverForTesting !== null);
    assertTrue(contextualTasksApp.tooltipResizeObserverForTesting !== null);

    // Verify resizeObserver_ still works.
    Object.defineProperty(innerComposebox, 'offsetHeight', {
      writable: true,
      configurable: true,
      value: 500,
    });

    // Trigger all resize observers.
    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    assertEquals(500, composeboxElement.composeboxHeightForTesting);
  });

  test('TooltipImpressionIncrementsAfterDelay', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = contextualTasksApp.$.onboardingTooltip;

    // Force show tooltip with delay.
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
      composeboxShowOnboardingTooltipImpressionDelay: 3000,
    });
    contextualTasksApp.numberOfTimesTooltipShownForTesting = 0;
    contextualTasksApp.userDismissedTooltipForTesting = false;

    const innerComposebox = composeboxElement.$.composebox;
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    // Trigger update.
    contextualTasksApp.updateTooltipVisibilityForTesting();
    assertTrue(tooltip!.shouldShow);

    // Should not have incremented yet.
    assertEquals(0, contextualTasksApp.numberOfTimesTooltipShownForTesting);

    // Tick almost to the end.
    mockTimer.tick(2999);
    assertEquals(0, contextualTasksApp.numberOfTimesTooltipShownForTesting);

    // Tick past the delay.
    mockTimer.tick(1);
    assertEquals(1, contextualTasksApp.numberOfTimesTooltipShownForTesting);
  });
  // </if>


  test('ToolChipVisibilityBasedOnInputState', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;

    const getChip = () => {
      const toolChip = $$(
          innerComposebox,
          '.context-menu-container:not(#voiceToolChipsContainer) cr-composebox-tool-chip');
      return toolChip ? $$(toolChip, '#toolEnabledButton') : null;
    };

    // Initial state: No tool active.
    await setActiveTool(ToolMode.kUnspecified);

    assertFalse(isVisible(getChip()));

    // Activate Deep Search.
    await setActiveTool(ToolMode.kDeepSearch);

    assertTrue(isVisible(getChip()), 'Deep search does not exist');
    assertTrue(
        getChip()!.textContent.includes('Deep Search'),
        'Deep search is not the text');

    // Activate Image Gen (nanoBananaChip).
    await setActiveTool(ToolMode.kImageGen);

    assertTrue(isVisible(getChip()), 'Create images does not exist');
    assertTrue(
        getChip()!.textContent.includes('Create images'),
        'Create images is not the text');

    // Activate Canvas.
    await setActiveTool(ToolMode.kCanvas);

    assertTrue(isVisible(getChip()), 'Canvas does not exist');
    assertTrue(
        getChip()!.textContent.includes('Canvas'), 'Canvas is not the text');

    // Back to Unspecified.
    await setActiveTool(ToolMode.kUnspecified);

    assertFalse(isVisible(getChip()), 'Tool chip still visible');
  });

  test(
      'composebox remains visible during results-to-results navigation',
      async () => {
        // Setup: Start in a loaded results state (not initial load, not zero state).
        contextualTasksApp.setIsInitialFrameLoadForTesting(false);
        contextualTasksApp.setIsZeroStateForTesting(false);
        window.dispatchEvent(new MessageEvent('message', {
          data: 'domContentLoaded',
        }));
        await contextualTasksApp.updateComplete;

        const composebox =
            contextualTasksApp.shadowRoot.querySelector('#composebox');
        assertTrue(!!composebox);
        assertEquals('visible', window.getComputedStyle(composebox).visibility);

        const mockEvent = {
          url: 'https://google.com/?q=results2',
          isTopLevel: true,
        } as unknown as chrome.webviewTag.LoadStartEvent;

        testProxy.handler.setIsAiPage(true);
        testProxy.handler.setIsZeroState(false);

        let resolver: () => void;
        const navFinished = new Promise<void>(resolve => {
          resolver = resolve;
        });
        contextualTasksApp.setOnLoadStartFinishedCallbackForTesting(() => {
          resolver();
        });

        // Trigger navigation flow.
        contextualTasksApp.onThreadFrameLoadStartForTesting(mockEvent);
        contextualTasksApp.onThreadFrameLoadCommitForTesting(mockEvent);

        // Wait for navigation to finish.
        await navFinished;
        await contextualTasksApp.updateComplete;

        // After check resolves (still results):
        assertEquals(
            'visible', window.getComputedStyle(composebox).visibility);

        // Simulate DOM load.
        window.dispatchEvent(new MessageEvent('message', {
          data: 'domContentLoaded',
        }));
        await contextualTasksApp.updateComplete;
        await microtasksFinished();

        assertEquals(
            'visible', window.getComputedStyle(composebox).visibility);
      });

  test(
      'composebox is hidden during subsequent navigation to zero-state until DOM loads',
      async () => {
        // Setup: Start in a loaded results state (not initial load, not zero state).
        contextualTasksApp.setIsInitialFrameLoadForTesting(false);
        contextualTasksApp.setIsZeroStateForTesting(false);
        window.dispatchEvent(new MessageEvent('message', {
          data: 'domContentLoaded',
        }));
        await contextualTasksApp.updateComplete;

        const composebox =
            contextualTasksApp.shadowRoot.querySelector('#composebox');
        assertTrue(!!composebox);
        assertEquals('visible', window.getComputedStyle(composebox).visibility);

        const mockEvent = {
          url: 'https://google.com/?gsc=2',
          isTopLevel: true,
        } as unknown as chrome.webviewTag.LoadStartEvent;

        testProxy.handler.setIsAiPage(true);
        testProxy.handler.setIsZeroState(true);

        let resolver: () => void;
        const navFinished = new Promise<void>(resolve => {
          resolver = resolve;
        });
        contextualTasksApp.setOnLoadStartFinishedCallbackForTesting(() => {
          resolver();
        });

        // Trigger navigation flow.
        contextualTasksApp.onThreadFrameLoadStartForTesting(mockEvent);
        contextualTasksApp.onThreadFrameLoadCommitForTesting(mockEvent);

        // Wait for navigation to finish.
        await navFinished;
        await contextualTasksApp.updateComplete;

        // Hidden after check resolves to zero-state.
        assertEquals(
            'hidden', window.getComputedStyle(composebox).visibility);

        // Simulate DOM load.
        window.dispatchEvent(new MessageEvent('message', {
          data: 'domContentLoaded',
        }));
        await contextualTasksApp.updateComplete;
        await microtasksFinished();

        assertEquals(
            'visible', window.getComputedStyle(composebox).visibility);
      });

  test('queries autocomplete on load when isZeroState is true', async () => {
    // Clear the body and reset the mock to test a fresh instance.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockSearchboxPageHandler.reset();
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    mockSearchboxPageHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
      },
    }));

    loadTimeData.overrideValues({composeboxShowZps: false});

    const app = document.createElement('contextual-tasks-app');

    testProxy.callbackRouterRemote.onZeroStateChange(false);

    document.body.appendChild(app);
    await app.updateComplete;
    await microtasksFinished();

    // Reset so that way any calls that happen before
    // adding to document do not count (since before that,
    // we are just setting up the test).
    mockSearchboxPageHandler.reset();

    // Mock `isZeroState_` updating value from parent.
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertEquals(
        1,
        mockSearchboxPageHandler.getCallCount(
            'queryAutocompleteWithSuggestInventory'));
  });

  test(
      'does not query autocomplete on load when isZeroState is false',
      async () => {
        // Clear the body and reset the mock to test a fresh instance.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        mockSearchboxPageHandler.reset();
        mockSearchboxPageHandler.setResultFor(
            'getPageClassification',
            Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
        mockSearchboxPageHandler.setResultFor('getInputState', Promise.resolve({
          state: {
            allowedModels: [],
            allowedTools: [],
            allowedInputTypes: [],
            activeModel: 0,
            activeTool: 0,
            disabledModels: [],
            disabledTools: [],
            disabledInputTypes: [],
          },
        }));

        loadTimeData.overrideValues({composeboxShowZps: false});

        const app = document.createElement('contextual-tasks-app');

        testProxy.callbackRouterRemote.onZeroStateChange(false);

        document.body.appendChild(app);

        await app.updateComplete;
        await microtasksFinished();

        // Reset so that way any calls that happen before
        // adding to document do not count (since before that,
        // we are just setting up the test).
        mockSearchboxPageHandler.reset();

        // Mock `isZeroState_` updating value from parent.
        testProxy.callbackRouterRemote.onZeroStateChange(false);

        assertEquals(
            0,
            mockSearchboxPageHandler.getCallCount(
                'queryAutocompleteWithSuggestInventory'));
      });

  test('typing clears suggestInventory', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;
    const inputElement = innerComposebox.getInputElement().$.input;

    // Set some non-default suggest inventory.
    innerComposebox.suggestInventory = SuggestInventory.kTravel;
    assertEquals(SuggestInventory.kTravel, innerComposebox.suggestInventory);

    // Simulate typing.
    simulateUserInput(inputElement, 'new query');
    mockTimer.tick(300);  // Trigger debounced query.

    // Verify suggestInventory is cleared.
    assertEquals(null, innerComposebox.suggestInventory);

    // Verify that the query call passed the default inventory.
    await mockSearchboxPageHandler.whenCalled(
        'queryAutocompleteWithSuggestInventory');
    const calls = mockSearchboxPageHandler.getArgs(
        'queryAutocompleteWithSuggestInventory');
    const lastCall = calls[calls.length - 1];
    assertEquals('new query', lastCall[0]);
    assertEquals(SuggestInventory.kDefault, lastCall[3]);
  });

  test('inputEnabled attribute reflected on composebox', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;

    // Default state should be enabled
    assertTrue(contextualComposebox.inputEnabled);
    assertTrue(contextualComposebox.hasAttribute('input-enabled'));

    // Disable input
    contextualComposebox.inputEnabled = false;
    await contextualComposebox.updateComplete;

    assertFalse(contextualComposebox.hasAttribute('input-enabled'));

    // Enable input
    contextualComposebox.inputEnabled = true;
    await contextualComposebox.updateComplete;

    assertTrue(contextualComposebox.hasAttribute('input-enabled'));
  });

  // Test that the Tab key correctly synchronizes the selected index.
  test('TabFocusSyncsSelectedIndex', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const dropdown = contextualComposebox.$.contextualTasksSuggestionsContainer;

    // Simulate focus moving to the first match (index 0) via Tab key.
    dropdown.dispatchEvent(new CustomEvent('match-focusin', {
      detail: {index: 0},
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();

    // Verify the index is synced in both the parent and the dropdown.
    assertEquals(0, contextualComposebox.selectedMatchIndexForTesting);
    assertEquals(0, dropdown.selectedMatchIndex);
  });

  test('TabFocusPopulatesTextAndEnterSubmits', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const dropdown = contextualComposebox.$.contextualTasksSuggestionsContainer;
    const innerComposebox = contextualComposebox.$.composebox;

    // Setup mock zero-state results.
    const matches = [
      createAutocompleteMatch(
          {contents: 'focus match', destinationUrl: 'https://test.com'}),
    ];
    contextualComposebox.zeroStateSuggestionsForTesting =
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        });

    innerComposebox.suggestInventory = SuggestInventory.kTravel;

    // Simulate Tab focus (match-focusin).
    dropdown.dispatchEvent(new CustomEvent('match-focusin', {
      detail: {index: 0},
      bubbles: true,
      composed: true,
    }));

    await innerComposebox.updateComplete;
    // Focusing on a suggestion should not clear suggestInventory.
    assertEquals(SuggestInventory.kTravel, innerComposebox.suggestInventory);

    // Simulate pressing Enter to submit.
    dropdown.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Enter',
      bubbles: true,
      composed: true,
    }));

    // Verify the Mojo handler was called correctly.
    const [index, url] =
        await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');
    assertEquals(0, index);
    assertEquals('https://test.com', url);

    // After submission, verify the input is cleared by your component logic.
    await innerComposebox.updateComplete;
    assertEquals('', innerComposebox.input);
    assertEquals(
        null, innerComposebox.getDropdownElement().result,
        'Matches should be cleared after submit');
  });

  test('OfflineStatusReconsideredOnReload', async () => {
    // 1. Initial state: Online.
    Object.defineProperty(window.navigator, 'onLine', {
      get: () => true,
      configurable: true,
    });

    const threadFrame = contextualTasksApp.$.threadFrame;
    const composebox = contextualTasksApp.$.composebox;

    mockSearchboxPageHandler.reset();
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));

    // Set to zero state to ensure autocomplete is queried.
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Simulate a load to initialize state.
    const loadStartEventOnline =
        new Event('loadstart') as Event & {isTopLevel?: boolean, url?: string};
    loadStartEventOnline.isTopLevel = true;
    loadStartEventOnline.url = fixtureUrl;
    threadFrame.dispatchEvent(loadStartEventOnline);

    await contextualTasksApp.updateComplete;

    assertFalse(
        contextualTasksApp.isLoadErrorForTesting, 'Should be online initially');
    assertTrue(isVisible(composebox), 'Composebox should be visible initially');
    assertEquals(
        1,
        mockSearchboxPageHandler.getCallCount(
            'queryAutocompleteWithSuggestInventory'));

    // 2. Go offline.
    Object.defineProperty(window.navigator, 'onLine', {
      get: () => false,
      configurable: true,
    });

    // Verify it's still visible because no reload happened yet.
    assertFalse(
        contextualTasksApp.isLoadErrorForTesting,
        'isLoadError_ should still be false before reload');
    assertTrue(
        isVisible(composebox),
        'Composebox should still be visible before reload');

    // 3. Simulate reload while offline.
    const loadStartEventOffline =
        new Event('loadstart') as Event & {isTopLevel?: boolean, url?: string};
    loadStartEventOffline.isTopLevel = true;
    loadStartEventOffline.url = fixtureUrl;
    threadFrame.dispatchEvent(loadStartEventOffline);

    await contextualTasksApp.updateComplete;

    assertTrue(
        contextualTasksApp.isLoadErrorForTesting,
        'Should be error after reload');
    assertFalse(
        isVisible(composebox),
        'Composebox should be hidden when in error state');

    // 4. Go back online.
    Object.defineProperty(window.navigator, 'onLine', {
      get: () => true,
      configurable: true,
    });

    // Verify it's still hidden because no reload happened yet.
    assertTrue(
        contextualTasksApp.isLoadErrorForTesting,
        'isLoadError_ should still be true before reload');
    assertFalse(
        isVisible(composebox),
        'Composebox should still be hidden before reload');

    // 5. Simulate reload while online.
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    const loadStartEventBackOnline =
        new Event('loadstart') as Event & {isTopLevel?: boolean, url?: string};
    loadStartEventBackOnline.isTopLevel = true;
    loadStartEventBackOnline.url = fixtureUrl;
    threadFrame.dispatchEvent(loadStartEventBackOnline);

    await contextualTasksApp.updateComplete;

    assertFalse(
        contextualTasksApp.isLoadErrorForTesting,
        'Should be online after reload');
    assertTrue(
        isVisible(composebox), 'Composebox should be visible after reload');
  });

  test('CanvasChipRemovabilityBasedOnQuerySubmission', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;

    const getChip = () => {
      const toolChip = $$(
          innerComposebox,
          '.context-menu-container:not(#voiceToolChipsContainer) cr-composebox-tool-chip');
      return toolChip ? $$(toolChip, '#toolEnabledButton') : null;
    };

    // Activate Canvas.
    await setActiveTool(ToolMode.kCanvas);

    const toolChip = getChip();
    assertTrue(isVisible(toolChip), 'Canvas chip should be visible');
    if (!toolChip) {
      return;
    }
    assertFalse(
        toolChip.classList.contains('unremovable'),
        'Canvas chip should not be unremovable initially');

    // Simulate C++ sending InputState with isCanvasQuerySubmitted = false.
    const inputStateNoRc = new MockInputState({
      allowedTools: [ToolMode.kCanvas],
      activeTool: ToolMode.kCanvas,
      isCanvasQuerySubmitted: false,
    });
    searchboxCallbackRouterRemote.onInputStateChanged(inputStateNoRc);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await innerComposebox.updateComplete;
    const toolChipNoRcObj = $$(innerComposebox, 'cr-composebox-tool-chip') as
        ComposeboxToolChipElement;
    if (toolChipNoRcObj) {
      await toolChipNoRcObj.updateComplete;
    }

    const toolChipNoRc = getChip();
    assertTrue(isVisible(toolChipNoRc), 'Canvas chip should be visible');
    if (!toolChipNoRc) {
      return;
    }
    assertFalse(
        toolChipNoRc.classList.contains('unremovable'),
        'Canvas chip should not be unremovable after non-query navigation');

    // Simulate C++ sending InputState with isCanvasQuerySubmitted = true.
    const inputStateWithRc = new MockInputState({
      allowedTools: [ToolMode.kCanvas],
      activeTool: ToolMode.kCanvas,
      isCanvasQuerySubmitted: true,
    });
    searchboxCallbackRouterRemote.onInputStateChanged(inputStateWithRc);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await innerComposebox.updateComplete;
    const toolChipWithRcObj = $$(innerComposebox, 'cr-composebox-tool-chip') as
        ComposeboxToolChipElement;
    if (toolChipWithRcObj) {
      await toolChipWithRcObj.updateComplete;
    }

    const toolChipWithRc = getChip();
    assertTrue(isVisible(toolChipWithRc), 'Canvas chip should be visible');
    if (!toolChipWithRc) {
      return;
    }
    assertTrue(
        toolChipWithRc.classList.contains('unremovable'),
        'Canvas chip should be unremovable after query context');

    // Verify cannot remove.
    let eventFired = false;
    innerComposebox.addEventListener('tool-click', () => {
      eventFired = true;
    });

    getChip()!.click();
    await microtasksFinished();
    assertFalse(eventFired, 'Event should not be fired for unremovable chip');

    // Reset to zero state.
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;

    // Verify state reset.
    assertFalse(contextualTasksApp.$.composebox.isCanvasQuerySubmitted());
  });

  test('SidePanelComposeboxAlignsStart', async () => {
    const composeboxElement = contextualTasksApp.$.composebox;
    const innerComposebox = composeboxElement.$.composebox;

    // Force side panel mode and display
    composeboxElement.isSidePanel = true;
    innerComposebox.style.display = 'block';
    await composeboxElement.updateComplete;
    await innerComposebox.updateComplete;
    await microtasksFinished();

    // Mock specific container bounds to guarantee enough space
    const container = composeboxElement.shadowRoot.querySelector<HTMLElement>(
        '#composeboxContainer');
    assertTrue(container !== null);
    container.style.width = '800px';
    innerComposebox.style.width = '400px';

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    await microtasksFinished();

    // Mathematically assert that the inner composebox aligns to the start
    const containerRect = container.getBoundingClientRect();
    const boxRect = innerComposebox.getBoundingClientRect();

    const leftSpace = boxRect.left - containerRect.left;

    // If it is aligned left, the space on the left should be 0.
    assertEquals(
        0, leftSpace,
        'Composebox should align to the left side of the side panel container');
  });

  test('AutoSuggestedTabTitleUpdates', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;

    // Initial tab suggestion
    const tabInfo = {
      tabId: 1,
      title: 'Initial Title',
      url: 'https://example.com',
      lastActive: {internalValue: BigInt(100)},
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
    };
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tabInfo);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Wait for files to populate
    await innerComposebox.updateComplete;
    let files = Array.from(innerComposebox.files.values()) as any[];
    assertEquals(1, files.length);
    const initialFile = files[0];
    assertEquals('Initial Title', initialFile.name);
    assertEquals(1, initialFile.tabId);
    assertEquals('https://example.com', initialFile.url);

    // Suggest identical tab but updated title
    const updatedTabInfo = {
      ...tabInfo,
      title: 'Updated Title',
    };
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(updatedTabInfo);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    files = Array.from(innerComposebox.files.values()) as any[];
    assertEquals(1, files.length);
    const updatedFile = files[0];
    assertEquals('Updated Title', updatedFile.name);
    assertEquals(initialFile.uuid, updatedFile.uuid);
    assertEquals(initialFile.tabId, updatedFile.tabId);
    assertEquals(initialFile.url, updatedFile.url);
    assertEquals(initialFile.status, updatedFile.status);
    assertEquals(initialFile.type, updatedFile.type);
    assertEquals(initialFile.inputType, updatedFile.inputType);

    // Suggest identical tab (same url/tabId), identical title, but different
    // lastActive
    const noUpdateTabInfo = {
      ...updatedTabInfo,
      lastActive: {internalValue: BigInt(500)},
    };
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
        noUpdateTabInfo);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    files = Array.from(innerComposebox.files.values()) as any[];
    assertEquals(1, files.length);
    // Reference should be exactly the same (no re-allocation or modification)
    assertEquals(updatedFile, files[0]);
  });

  test('SmartTabSharingActiveChangedFiresMojo', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const crComposebox = $$(contextualComposebox, '#composebox');
    assertTrue(!!crComposebox);
    const entrypointAndMenu =
        $$(crComposebox, 'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);

    mockComposeboxPageHandler.reset();

    entrypointAndMenu.dispatchEvent(
        new CustomEvent('smart-tab-sharing-active-changed', {
          detail: {active: true},
          bubbles: true,
          composed: true,
        }));

    const activeArg =
        await mockComposeboxPageHandler.whenCalled('setSmartTabSharingActive');
    assertEquals(
        1, mockComposeboxPageHandler.getCallCount('setSmartTabSharingActive'));
    assertEquals(true, activeArg);
  });

  test('ContextMenuOpenedFiresMojo', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const crComposebox = $$(contextualComposebox, '#composebox');
    assertTrue(!!crComposebox);
    const entrypointAndMenu =
        $$(crComposebox, 'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);

    mockComposeboxPageHandler.reset();

    entrypointAndMenu.dispatchEvent(new CustomEvent('context-menu-opened', {
      bubbles: true,
      composed: true,
    }));

    await mockComposeboxPageHandler.whenCalled('onContextMenuOpened');
    assertEquals(
        1, mockComposeboxPageHandler.getCallCount('onContextMenuOpened'));
  });

  test('VoiceSearchErrorDetailsLinkIsClickable', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    contextualComposebox.style.pointerEvents = 'none';

    const voiceSearchElement =
        innerComposebox.shadowRoot.querySelector('cr-composebox-voice-search');
    assertTrue(!!voiceSearchElement, 'Voice search element should exist');

    // Trigger a NO_MATCH error to display the error details link.
    (voiceSearchElement as unknown as {
      onError_: (e: number) => void,
    }).onError_(5);
    await microtasksFinished();

    const detailsLink =
        voiceSearchElement.shadowRoot.querySelector<HTMLAnchorElement>(
            '#details');
    assertTrue(!!detailsLink, 'Error details link should be rendered');

    const computedStyle = window.getComputedStyle(detailsLink);
    assertEquals(
        'auto', computedStyle.pointerEvents,
        'Details link must have pointer-events: auto despite parent restrictions');

    let cancelEventFired = false;
    let isCanceledByUser = true;
    voiceSearchElement.addEventListener('voice-search-cancel', (e: Event) => {
      cancelEventFired = true;
      isCanceledByUser = (e as CustomEvent<boolean>).detail;
    });

    detailsLink.click();
    await microtasksFinished();

    assertEquals(
        1, mockComposeboxPageHandler.getCallCount('navigateUrl'),
        'navigateUrl should be called exactly once');

    const navigatedUrl =
        await mockComposeboxPageHandler.whenCalled('navigateUrl');

    assertTrue(
        typeof navigatedUrl === 'string' &&
            navigatedUrl.includes('support.google.com'),
        'Should navigate to the correct Chrome support page');

    assertTrue(cancelEventFired, 'voice-search-cancel event should be fired');
    assertFalse(
        isCanceledByUser,
        'Cancel event should indicate it was not canceled by user');

    assertFalse(
        (voiceSearchElement as unknown as {
          shouldShowErrorScrim_: () => boolean,
        }).shouldShowErrorScrim_(),
        'Error scrim should hide after clicking the details link');
  });

  // Required to test how the voice chips are integrated into contextual
  // tasks html (event listeners, CSS ID's, CSS classes, etc.):
  suite('voice search', () => {
    setup(async () => {
      const windowProxy = TestMock.fromClass(WindowProxy);
      windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
      windowProxy.setResultMapperFor('createSpeechRecognition', () => {
        const mock = new EventTarget() as unknown as
            ReturnType<typeof WindowProxy.prototype.createSpeechRecognition>;
        mock.abort = () => {};
        mock.start = () => {};
        mock.stop = () => {};
        return mock;
      });
      windowProxy.setResultMapperFor(
          'matchMedia', (query: string) => window.matchMedia(query));
      WindowProxy.setInstance(windowProxy);

      mockSearchboxPageHandler.setPromiseResolveFor('getPageClassification', {
        metricSource: 'CO_BROWSING_COMPOSEBOX',
      });

      composebox.showVoiceSearch = true;
      await composebox.updateComplete;
    });

    async function enterVoiceSearchMode() {
      const voiceSearchButton =
          composebox.shadowRoot.querySelector('#voiceSearchButton');
      assertTrue(!!voiceSearchButton);
      voiceSearchButton.click();
      await microtasksFinished();
      await composebox.updateComplete;
      // Must wait for the second part of voice search to render as well.
      const animatedGlow =
          composebox.shadowRoot.querySelector('search-animated-glow');
      if (animatedGlow) {
        await animatedGlow.updateComplete;
      }
    }

    async function submitVoiceSearch() {
      const voiceSearch =
          composebox.shadowRoot.querySelector('cr-composebox-voice-search');
      assertTrue(!!voiceSearch);

      const mockVoiceSearch = voiceSearch as unknown as {
        finalResult_: string,
        transcript_: string,
      };
      mockVoiceSearch.finalResult_ = 'test query';
      mockVoiceSearch.transcript_ = 'test query';
      voiceSearch.requestUpdate();
      await voiceSearch.updateComplete;

      const submitButton =
          voiceSearch.shadowRoot.querySelector('cr-composebox-submit');
      assertTrue(!!submitButton);
      await submitButton.updateComplete;

      const submitContainer =
          submitButton.shadowRoot.querySelector('#submitContainer');
      assertTrue(!!submitContainer);
      submitContainer.click();

      await microtasksFinished();
      await composebox.updateComplete;
      await mockSearchboxPageHandler.whenCalled('submitQuery');
    }

    test(
        'voice error scrim is absolute when not hidden; display none otherwise',
        async () => {
          // When no error: errorScrim should be absent:
          let errorScrim = composebox.shadowRoot.querySelector('#errorScrim');
          assertFalse(!!errorScrim);

          // When error: errorScrim is shown, must be position absolute:
          composebox.inVoiceSearchMode = true;
          composebox.errorMessage = 'Network error';
          await composebox.updateComplete;

          errorScrim = composebox.shadowRoot.querySelector('#errorScrim');
          assertTrue(!!errorScrim);
          assertEquals(
              'absolute', window.getComputedStyle(errorScrim).position);

          // When dismissed (hidden again):
          const shadowRoot = errorScrim.shadowRoot;
          assertTrue(!!shadowRoot);
          if (!shadowRoot) {
            return;
          }
          const dismissErrorButton =
              shadowRoot.querySelector('#dismissErrorButton');
          assertTrue(!!dismissErrorButton);
          dismissErrorButton.click();
          await microtasksFinished();
          await composebox.updateComplete;

          errorScrim = composebox.shadowRoot.querySelector('#errorScrim');
          // Equivalent to checking 'display none':
          assertFalse(!!errorScrim);
        });

    test('toolchip and image added, then removed in voice search', async () => {
      // Add tool chip:
      composebox.contextMenuEnabled = true;
      composebox.inToolMode = true;
      composebox.voiceSearchCoherenceEnabled = true;

      // Add image:
      const thumbnailUrl = 'data:image/png;base64,sometestdata';
      const testToken = '12345678901234567890123456789012';
      searchboxCallbackRouterRemote.addFileContext(testToken, {
        fileName: 'test.png',
        mimeType: 'image/png',
        imageDataUrl: thumbnailUrl,
        isDeletable: true,
        selectionTime: new Date(),
      } as SelectedFileInfo);
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      await composebox.updateComplete;

      // Enter voice search mode:
      await enterVoiceSearchMode();

      // Ensure carousel and toolchip are visible in voice search:
      const animatedGlow =
          composebox.shadowRoot.querySelector('search-animated-glow');
      assertTrue(!!animatedGlow);
      const voiceCarouselContainer =
          animatedGlow.querySelector('#voiceCarouselContainer');
      assertTrue(!!voiceCarouselContainer);
      const voiceCarousel =
          voiceCarouselContainer.querySelector('#voiceSearchCarousel');
      assertTrue(!!voiceCarousel);
      const voiceToolChip =
          animatedGlow.querySelector('#voiceToolChipsContainer');
      assertTrue(!!voiceToolChip);

      // Verify CSS order
      assertTrue(voiceCarousel.classList.contains('top'));
      assertEquals('0', window.getComputedStyle(voiceCarouselContainer).order);
      assertEquals('3', window.getComputedStyle(voiceToolChip).order);
      const recordingWave =
          animatedGlow.shadowRoot.querySelector('#recordingWave');
      assertTrue(!!recordingWave);
      assertEquals('1', window.getComputedStyle(recordingWave).order);

      // Remove image:
      const shadowRoot = voiceCarousel.shadowRoot;
      assertTrue(!!shadowRoot);
      if (!shadowRoot) {
        return;
      }
      const fileThumbnail =
          shadowRoot.querySelector('cr-composebox-file-thumbnail');
      assertTrue(!!fileThumbnail);
      const removeImgButton =
          fileThumbnail.shadowRoot.querySelector('#removeImgButton');
      removeImgButton.click();
      await microtasksFinished();
      await composebox.updateComplete;
      assertEquals(0, composebox.files.size);

      // Remove toolchip:
      composebox.inToolMode = false;
      await composebox.updateComplete;
      assertFalse(!!animatedGlow.querySelector('#voiceToolChipsContainer'));
    });

    test('remove image but submit toolchip in voice search mode', async () => {
      // Add tool chip and image:
      composebox.contextMenuEnabled = true;
      composebox.inToolMode = true;
      composebox.voiceSearchCoherenceEnabled = true;
      const thumbnailUrl = 'data:image/png;base64,sometestdata';
      const testToken = '12345678901234567890123456789012';
      searchboxCallbackRouterRemote.addFileContext(testToken, {
        fileName: 'test.png',
        mimeType: 'image/png',
        imageDataUrl: thumbnailUrl,
        isDeletable: true,
        selectionTime: new Date(),
      } as SelectedFileInfo);
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      await composebox.updateComplete;

      await enterVoiceSearchMode();

      const animatedGlow =
          composebox.shadowRoot.querySelector('search-animated-glow');
      assertTrue(!!animatedGlow);
      const voiceCarouselContainer =
          animatedGlow.querySelector('#voiceCarouselContainer');
      assertTrue(!!voiceCarouselContainer);
      const voiceCarousel =
          voiceCarouselContainer.querySelector('#voiceSearchCarousel');
      assertTrue(!!voiceCarousel);

      // Remove image from voice carousel:
      const shadowRoot = voiceCarousel.shadowRoot;
      assertTrue(!!shadowRoot);
      if (!shadowRoot) {
        return;
      }
      const fileThumbnail =
          shadowRoot.querySelector('cr-composebox-file-thumbnail');
      assertTrue(!!fileThumbnail);
      const removeImgButton =
          fileThumbnail.shadowRoot.querySelector('#removeImgButton');
      removeImgButton.click();
      await microtasksFinished();
      await composebox.updateComplete;
      assertEquals(0, composebox.files.size);

      // Submit:
      await submitVoiceSearch();

      assertTrue(composebox.inToolMode);
      assertEquals(0, composebox.files.size);
    });

    test('remove toolchip but submit image in voice search mode', async () => {
      // Add tool chip and image:
      composebox.contextMenuEnabled = true;
      composebox.inToolMode = true;
      composebox.voiceSearchCoherenceEnabled = true;
      const thumbnailUrl = 'data:image/png;base64,sometestdata';
      const testToken = '12345678901234567890123456789012';
      searchboxCallbackRouterRemote.addFileContext(testToken, {
        fileName: 'test.png',
        mimeType: 'image/png',
        imageDataUrl: thumbnailUrl,
        isDeletable: true,
        selectionTime: new Date(),
      } as SelectedFileInfo);
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      await composebox.updateComplete;

      await enterVoiceSearchMode();

      const animatedGlow =
          composebox.shadowRoot.querySelector('search-animated-glow');
      assertTrue(!!animatedGlow);
      const voiceToolChip =
          animatedGlow.querySelector('#voiceToolChipsContainer');
      assertTrue(!!voiceToolChip);

      // Remove tool chip from voice tool chips container:
      const toolChip = voiceToolChip.querySelector('cr-composebox-tool-chip');
      assertTrue(!!toolChip);
      const toolEnabledButton =
          toolChip.shadowRoot.querySelector('#toolEnabledButton');
      assertTrue(!!toolEnabledButton);
      toolEnabledButton.click();
      // Prevent the image file from being cleared on component
      // updates (follows `inputState`):
      searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState({
        activeTool: ToolMode.kUnspecified,
        allowedInputTypes: [InputType.kLensImage],
      }));
      await microtasksFinished();
      await composebox.updateComplete;
      assertFalse(composebox.inToolMode);
      assertEquals(1, composebox.files.size);

      // Submit:
      await submitVoiceSearch();

      assertFalse(composebox.inToolMode);
      // Submitting resets file count to 0:
      assertEquals(0, composebox.files.size);
    });

    test(
        'removing chips in voice carousel removes them from main carousel' +
            ' after stopping recording',
        async () => {
          // Add tool chip and image:
          composebox.contextMenuEnabled = true;
          composebox.inToolMode = true;
          composebox.voiceSearchCoherenceEnabled = true;
          const thumbnailUrl = 'data:image/png;base64,sometestdata';
          const testToken = '12345678901234567890123456789012';
          searchboxCallbackRouterRemote.addFileContext(testToken, {
            fileName: 'test.png',
            mimeType: 'image/png',
            imageDataUrl: thumbnailUrl,
            isDeletable: true,
            selectionTime: new Date(),
          } as SelectedFileInfo);
          await searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();
          await composebox.updateComplete;

          // Enter voice search mode by clicking voice search button:
          await enterVoiceSearchMode();

          const animatedGlow =
              composebox.shadowRoot.querySelector('search-animated-glow');
          assertTrue(!!animatedGlow);
          const voiceCarouselContainer =
              animatedGlow.querySelector('#voiceCarouselContainer');
          assertTrue(!!voiceCarouselContainer);
          const voiceCarousel =
              voiceCarouselContainer.querySelector('#voiceSearchCarousel');
          assertTrue(!!voiceCarousel);
          const voiceToolChip =
              animatedGlow.querySelector('#voiceToolChipsContainer');
          assertTrue(!!voiceToolChip);

          // Remove image from voice carousel:
          const shadowRoot = voiceCarousel.shadowRoot;
          assertTrue(!!shadowRoot);
          if (!shadowRoot) {
            return;
          }
          const fileThumbnail =
              shadowRoot.querySelector('cr-composebox-file-thumbnail');
          assertTrue(!!fileThumbnail);
          const removeImgButton =
              fileThumbnail.shadowRoot.querySelector('#removeImgButton');
          removeImgButton.click();
          await microtasksFinished();
          await composebox.updateComplete;
          assertEquals(0, composebox.files.size);

          // Remove tool chip from voice tool chips container:
          const toolChip =
              voiceToolChip.querySelector('cr-composebox-tool-chip');
          assertTrue(!!toolChip);
          const toolEnabledButton =
              toolChip.shadowRoot.querySelector('#toolEnabledButton');
          assertTrue(!!toolEnabledButton);
          toolEnabledButton.click();
          searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState({
            activeTool: ToolMode.kUnspecified,
            allowedInputTypes: [InputType.kLensImage],
          }));
          await microtasksFinished();
          await composebox.updateComplete;
          assertFalse(composebox.inToolMode);

          // Stop recording:
          const voiceSearch =
              composebox.shadowRoot.querySelector('cr-composebox-voice-search');
          assertTrue(!!voiceSearch);
          const stopButton =
              voiceSearch.shadowRoot.querySelector('#stopButton');
          assertTrue(!!stopButton);
          stopButton.click();
          await microtasksFinished();
          await composebox.updateComplete;

          assertFalse(composebox.inToolMode);
          assertEquals(0, composebox.files.size);
        });

    test(
        'voice search and its container are absolute when not waiting ' +
            'and not in error',
        async () => {
          composebox.showVoiceSearch = true;
          await composebox.updateComplete;

          const voiceSearch =
              composebox.shadowRoot.querySelector('cr-composebox-voice-search');
          assertTrue(!!voiceSearch);

          // Not waiting and not in error:
          composebox.inVoiceSearchMode = true;
          composebox.isListening = true;
          await composebox.updateComplete;
          voiceSearch.isPermissionPromptOpen = false;
          await voiceSearch.updateComplete;

          const voiceSearchContainer =
              voiceSearch.shadowRoot.querySelector('#container');
          assertTrue(!!voiceSearchContainer);

          assertEquals(
              'absolute', window.getComputedStyle(voiceSearch).position);
          assertEquals(
              'absolute',
              window.getComputedStyle(voiceSearchContainer).position);

          // Waiting (permission prompt open):
          voiceSearch.isPermissionPromptOpen = true;
          await voiceSearch.updateComplete;
          assertNotEquals(
              'absolute',
              window.getComputedStyle(voiceSearchContainer).position);

          // In error:
          voiceSearch.isPermissionPromptOpen = false;
          (voiceSearch as unknown as {errorMessage_: string}).errorMessage_ =
              'Voice error';
          await voiceSearch.updateComplete;
          assertNotEquals(
              'absolute',
              window.getComputedStyle(voiceSearchContainer).position);
        });
  });
});

// =============================================================================
// Fork DUAL-PATH SMOKE SUITE
// Infrastructure-only coverage: verifies the wrapper's
// `useContextualTasksComposeboxFork` ternary picks the right inner element
// on both paths. The fork is a smoke skeleton, so nothing here may depend on
// inner composebox behavior.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
     `ContextualTasksComposeboxForkSmokeTest (useContextualTasksComposeboxFork =
        ${useFork})`,
      () => {
        let testProxy: TestContextualTasksBrowserProxy;
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let parts: CtComposeboxAppParts;

        setup(async () => {
          if (!window.chrome) {
            Object.assign(window, {chrome: {}});
          }

          if (!window.chrome.histograms) {
            Object.assign(window.chrome, {
              histograms: {
                recordEnumerationValue: () => {},
                recordUserAction: () => {},
                recordBoolean: () => {},
              },
            });
          }
          document.body.innerHTML = window.trustedTypes!.emptyHTML;

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
            forcedEmbeddedPageHost: '',
            tabFaviconChipsToCoinsEnabled: false,
          });

          testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
          BrowserProxyImpl.setInstance(testProxy);

          mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'addTabContext',
              Promise.resolve({high: BigInt(1), low: BigInt(2)}));
          mockSearchboxPageHandler.setResultFor(
              'getInputState', Promise.resolve({state: new MockInputState()}));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, new ComposeboxPageCallbackRouter(),
              mockSearchboxPageHandler, searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
        });

        test('flag selects the expected inner composebox element', () => {
          const {wrapper, innerComposebox} = parts;
          assertEquals(
              useFork ? 'CONTEXTUAL-TASKS-INNER-COMPOSEBOX' : 'CR-COMPOSEBOX',
              innerComposebox.tagName);
          assertEquals('composebox', innerComposebox.id);
          assertEquals(
            innerComposebox,
            wrapper.shadowRoot.querySelector('#composebox'));
        });

        test('wrapper tracks focus state from inner composebox events',
             async () => {
               const {wrapper, innerComposebox} = parts;

               innerComposebox.dispatchEvent(
                   new CustomEvent('composebox-focus-in'));
               await microtasksFinished();
               assertTrue(wrapper.isComposeboxFocusedForTesting);

               innerComposebox.dispatchEvent(
                   new CustomEvent('composebox-focus-out'));
               await microtasksFinished();
               assertFalse(wrapper.isComposeboxFocusedForTesting);
        });
      });
});

// =============================================================================
// Fork DUAL-PATH BASIC INPUT/SUBMIT/CLEAR SUITE
// Basic input, submit, and clear behavior is implemented by both the legacy
// <cr-composebox> and the <contextual-tasks-inner-composebox>, so these tests
// run on both paths. Tests depending on behavior the fork does not implement
// yet (selected-match submit, dropdown/result-changed, files, voice, ...) stay
// in the flag-off suites above.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkSmokeTest (useContextualTasksComposeboxFork =
        ${useFork})`,
      () => {
        let testProxy: TestContextualTasksBrowserProxy;
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let parts: CtComposeboxAppParts;

        setup(async () => {
          if (!window.chrome) {
            Object.assign(window, {chrome: {}});
          }

          if (!window.chrome.histograms) {
            Object.assign(window.chrome, {
              histograms: {
                recordEnumerationValue: () => {},
                recordUserAction: () => {},
                recordBoolean: () => {},
              },
            });
          }
          document.body.innerHTML = window.trustedTypes!.emptyHTML;

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
            composeboxHintTextLensOverlay: 'Test Lens Hint',
            forcedEmbeddedPageHost: '',
            tabFaviconChipsToCoinsEnabled: false,
          });

          testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
          BrowserProxyImpl.setInstance(testProxy);

          mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'addTabContext',
              Promise.resolve({high: BigInt(1), low: BigInt(2)}));
          mockSearchboxPageHandler.setResultFor(
              'getInputState', Promise.resolve({state: new MockInputState()}));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, new ComposeboxPageCallbackRouter(),
              mockSearchboxPageHandler, searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
        });

        test('EnterKeyOnEmptyInputDoesNotAddNewLineOrSubmit', async () => {
          const {innerComposebox} = parts;
          const inputElement = innerComposebox.getInputElement().$.input;
          const keydownDiv =
              innerComposebox.shadowRoot.querySelector<HTMLElement>(
                  '#composebox');
          assertTrue(keydownDiv !== null);

          assertEquals('', inputElement.value);
          mockSearchboxPageHandler.reset();

          // Action: Press Enter on empty input.
          pressEnter(keydownDiv);
          await microtasksFinished();

          // Assert: No newline and no submission.
          assertFalse(inputElement.value.includes('\n'));
          assertEquals(0, mockSearchboxPageHandler.getCallCount('submitQuery'));
        });

        test(
            'cancel button click clears input without submitting', async () => {
              const {innerComposebox} = parts;
              const inputElement = innerComposebox.getInputElement().$.input;
              const cancelIcon = innerComposebox.getInputElement().$.cancelIcon;

              // Type text so the composebox has content; with content present,
              // cancel clears the input instead of closing the composebox.
              simulateUserInput(inputElement, 'test query');
              await innerComposebox.updateComplete;
              assertEquals('test query', innerComposebox.input);

              // Reset so setup / initial ZPS calls do not pollute the counts
              // below.
              mockSearchboxPageHandler.reset();

              cancelIcon.click();
              await innerComposebox.updateComplete;
              await innerComposebox.getInputElement().updateComplete;

              // Cancel clears the input and its uploaded files, but never
              // submits.
              assertEquals('', innerComposebox.input);
              assertEquals('', inputElement.value);
              assertEquals(
                  0, mockSearchboxPageHandler.getCallCount('submitQuery'));
              assertEquals(
                  1, mockSearchboxPageHandler.getCallCount('clearFiles'));
            });

        test('lens overlay showing updates placeholder', async () => {
          const {wrapper, innerComposebox} = parts;
          const inputElement = innerComposebox.getInputElement().$.input;

          // Initially false, placeholder override should be empty.
          assertFalse(wrapper.isOverlayOpenForAimVisualSearch);
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;
          assertEquals('', innerComposebox.inputPlaceholderOverride);

          const initialPlaceholder = inputElement.placeholder;

          // Set to true.
          wrapper.isOverlayOpenForAimVisualSearch = true;
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;

          assertTrue(wrapper.isOverlayOpenForAimVisualSearch);
          assertEquals(
              'Test Lens Hint', innerComposebox.inputPlaceholderOverride);
          assertEquals('Test Lens Hint', inputElement.placeholder);

          // Set back to false.
          wrapper.isOverlayOpenForAimVisualSearch = false;
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;

          assertFalse(wrapper.isOverlayOpenForAimVisualSearch);
          assertEquals('', innerComposebox.inputPlaceholderOverride);
          assertEquals(initialPlaceholder, inputElement.placeholder);
        });

        test('ClearInputAndFocusClearsMatchesOnSubmit', () => {
          const {wrapper, innerComposebox} = parts;

          let clearAutocompleteMatchesCallCount = 0;
          let queryAutocompleteCallCount = 0;

          innerComposebox.clearAutocompleteMatches = () => {
            clearAutocompleteMatchesCallCount++;
          };

          innerComposebox.queryAutocomplete = () => {
            queryAutocompleteCallCount++;
          };

          wrapper.isZeroState = true;
          wrapper.clearInputAndFocus(true);
          assertEquals(
              1, clearAutocompleteMatchesCallCount,
              'querySubmitted = true should clear matches');
          assertEquals(
              0, queryAutocompleteCallCount,
              'querySubmitted = true should not query');
        });

        test('ClearInputAndFocusClearsMatchesWhenNotZeroState', () => {
          const {wrapper, innerComposebox} = parts;

          let clearAutocompleteMatchesCallCount = 0;
          let queryAutocompleteCallCount = 0;

          innerComposebox.clearAutocompleteMatches = () => {
            clearAutocompleteMatchesCallCount++;
          };

          innerComposebox.queryAutocomplete = () => {
            queryAutocompleteCallCount++;
          };

          wrapper.isZeroState = false;
          wrapper.clearInputAndFocus(false);
          assertEquals(
              1, clearAutocompleteMatchesCallCount,
              'isZeroState = false should clear matches');
          assertEquals(
              0, queryAutocompleteCallCount,
              'isZeroState = false should not query');
        });

        test('ClearInputAndFocusIgnoresEmptyZeroState', () => {
          const {wrapper, innerComposebox} = parts;

          let clearAutocompleteMatchesCallCount = 0;
          let queryAutocompleteCallCount = 0;

          innerComposebox.clearAutocompleteMatches = () => {
            clearAutocompleteMatchesCallCount++;
          };

          innerComposebox.queryAutocomplete = () => {
            queryAutocompleteCallCount++;
          };

          wrapper.isZeroState = true;
          innerComposebox.getInputElement().$.input.value = '';
          wrapper.clearInputAndFocus(false);
          assertEquals(
              0, clearAutocompleteMatchesCallCount,
              'hadContent = false should not clear matches');
          assertEquals(
              0, queryAutocompleteCallCount,
              'hadContent = false should not query');
        });

        test('ClearInputAndFocusQueriesZeroStateWithText', () => {
          const {wrapper, innerComposebox} = parts;

          let clearAutocompleteMatchesCallCount = 0;
          let queryAutocompleteCallCount = 0;
          let queryAutocompleteClearMatchesArg = false;

          innerComposebox.clearAutocompleteMatches = () => {
            clearAutocompleteMatchesCallCount++;
          };

          innerComposebox.queryAutocomplete = (clearMatches: boolean) => {
            queryAutocompleteCallCount++;
            queryAutocompleteClearMatchesArg = clearMatches;
          };

          wrapper.isZeroState = true;
          innerComposebox.input = 'test';
          wrapper.clearInputAndFocus(false);
          assertEquals(
              0, clearAutocompleteMatchesCallCount,
              'hadContent = true should not clear matches');
          assertEquals(
              1, queryAutocompleteCallCount, 'hadContent = true should query');
          assertTrue(
              queryAutocompleteClearMatchesArg,
              'should pass clearMatches = true');
        });

        test('ClearInputAndFocusQueriesZeroStateWithFiles', () => {
          const {wrapper, innerComposebox} = parts;

          let clearAutocompleteMatchesCallCount = 0;
          let queryAutocompleteCallCount = 0;
          let queryAutocompleteClearMatchesArg = false;

          innerComposebox.clearAutocompleteMatches = () => {
            clearAutocompleteMatchesCallCount++;
          };

          innerComposebox.queryAutocomplete = (clearMatches: boolean) => {
            queryAutocompleteCallCount++;
            queryAutocompleteClearMatchesArg = clearMatches;
          };

          wrapper.isZeroState = true;
          innerComposebox.input = '';
          innerComposebox.hasFiles = () => true;
          wrapper.clearInputAndFocus(false);
          assertEquals(
              0, clearAutocompleteMatchesCallCount,
              'hadContent = true (files) should not clear matches');
          assertEquals(
              1, queryAutocompleteCallCount,
              'hadContent = true (files) should query');
          assertTrue(
              queryAutocompleteClearMatchesArg,
              'should pass clearMatches = true');
        });
      });
});

// =============================================================================
// Fork DUAL-PATH DROPDOWN / RESULT-CHANGED / SUGGESTION-ACTIVITY SUITE
// The fork forwards autocomplete results to the wrapper via `result-changed`
// and signals the suggestion-activity link via `show-suggestion-activity-link`,
// matching the legacy <cr-composebox>, so these tests run on both paths.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkDropdownTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let searchboxCallbackRouterRemote: SearchboxPageRemote;
        let mockTimer: MockTimer;
        let parts: CtComposeboxAppParts;

        setup(async () => {
          if (!window.chrome) {
            Object.assign(window, {chrome: {}});
          }
          if (!window.chrome.histograms) {
            Object.assign(window.chrome, {
              histograms: {
                recordEnumerationValue: () => {},
                recordUserAction: () => {},
                recordBoolean: () => {},
              },
            });
          }
          document.body.innerHTML = window.trustedTypes!.emptyHTML;

          mockTimer = new MockTimer();

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
            composeboxHintTextLensOverlay: 'Test Lens Hint',
            forcedEmbeddedPageHost: '',
            tabFaviconChipsToCoinsEnabled: false,
          });

          const testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
          BrowserProxyImpl.setInstance(testProxy);

          mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'addTabContext',
              Promise.resolve({high: BigInt(1), low: BigInt(2)}));
          mockSearchboxPageHandler.setResultFor(
              'getInputState', Promise.resolve({state: new MockInputState()}));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouterRemote =
              searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, new ComposeboxPageCallbackRouter(),
              mockSearchboxPageHandler, searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
          searchboxCallbackRouterRemote.onInputStateChanged(
              new MockInputState());
          await microtasksFinished();
        });

        teardown(() => {
          mockTimer.uninstall();
        });

        test('fires result-changed for an accepted autocomplete result',
            async () => {
              mockTimer.install();
              const {innerComposebox} = parts;
              const inputElement = innerComposebox.getInputElement().$.input;
              const testQuery = 'test';

              simulateUserInput(inputElement, testQuery);
              mockTimer.tick(300);
              await mockSearchboxPageHandler.whenCalled(
                  'queryAutocompleteWithSuggestInventory');

              const whenResultChanged =
                  eventToPromise<CustomEvent<AutocompleteResult>>(
                      'result-changed', innerComposebox);
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    input: testQuery,
                    matches: [createAutocompleteMatch({fillIntoEdit: 'm1'})],
                  }));
              await searchboxCallbackRouterRemote.$.flushForTesting();
              mockTimer.tick(0);

              const event = await whenResultChanged;
              assertEquals(testQuery, event.detail.input);
              assertEquals(1, event.detail.matches.length);
            });

        test('does not fire result-changed for a stale autocomplete result',
            async () => {
              mockTimer.install();
              const {innerComposebox} = parts;
              const inputElement = innerComposebox.getInputElement().$.input;

              simulateUserInput(inputElement, 'test');
              mockTimer.tick(300);
              await mockSearchboxPageHandler.whenCalled(
                  'queryAutocompleteWithSuggestInventory');

              let fired = false;
              innerComposebox.addEventListener(
                  'result-changed', () => fired = true);

              // The response input does not match the last queried input.
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    input: 'stale',
                    matches: [createAutocompleteMatch()],
                  }));
              await searchboxCallbackRouterRemote.$.flushForTesting();
              mockTimer.tick(0);
              await innerComposebox.updateComplete;

              assertFalse(
                  fired, 'result-changed should not fire for a stale result');
            });

        test(
            'fires show-suggestion-activity-link for a noncanned AIM suggestion',
            async () => {
              const {innerComposebox} = parts;
              let lastDetail: boolean|null = null;
              innerComposebox.addEventListener(
                  'show-suggestion-activity-link',
                  e => lastDetail = (e as CustomEvent<boolean>).detail);

              // Zero-prefix-suggest results (empty input) keep the dropdown
              // shown; one match is a noncanned AIM suggestion.
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    input: '',
                    matches: [
                      createAutocompleteMatch({isNoncannedAimSuggestion: true}),
                      createAutocompleteMatch(),
                    ],
                  }));
              await searchboxCallbackRouterRemote.$.flushForTesting();
              await innerComposebox.updateComplete;

              assertTrue(
                  !!lastDetail,
                  'show-suggestion-activity-link should fire true');
            });

        test('clears the suggestion-activity link for ordinary results',
            async () => {
              const {wrapper, innerComposebox} = parts;
              let lastDetail: boolean|null = null;
              innerComposebox.addEventListener(
                  'show-suggestion-activity-link',
                  e => lastDetail = (e as CustomEvent<boolean>).detail);

              // A noncanned AIM suggestion first surfaces the link.
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    input: '',
                    matches: [
                      createAutocompleteMatch({isNoncannedAimSuggestion: true}),
                      createAutocompleteMatch(),
                    ],
                  }));
              await searchboxCallbackRouterRemote.$.flushForTesting();
              await innerComposebox.updateComplete;
              assertTrue(!!lastDetail);

              // Ordinary results clear it; the wrapper keeps no residual link.
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    input: '',
                    matches: [
                      createAutocompleteMatch(),
                      createAutocompleteMatch(),
                    ],
                  }));
              await searchboxCallbackRouterRemote.$.flushForTesting();
              await innerComposebox.updateComplete;
              await wrapper.updateComplete;

              assertFalse(
                  !!lastDetail,
                  'show-suggestion-activity-link should fire false');
              assertEquals(
                  null,
                  wrapper.shadowRoot.querySelector('#suggestionActivity'),
                  'wrapper should not keep a residual activity link');
            });

        test('selecting a match populates the composebox', async () => {
          mockTimer.install();
          const {innerComposebox} = parts;
          const inputElement = innerComposebox.getInputElement().$.input;
          const testQuery = 'test';

          simulateUserInput(inputElement, testQuery);
          searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                input: testQuery,
                matches: [
                  createAutocompleteMatch({fillIntoEdit: 'match 1'}),
                  createAutocompleteMatch({fillIntoEdit: 'match 2'}),
                ],
              }));
          await searchboxCallbackRouterRemote.$.flushForTesting();
          mockTimer.tick(0);

          const matchesEl = innerComposebox.getDropdownElement();
          assertTrue(matchesEl.result !== null, 'Matches should be populated');
          assertEquals(2, matchesEl.result.matches.length);

          inputElement.dispatchEvent(new KeyboardEvent(
              'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
          mockTimer.tick(100);
          await innerComposebox.getDropdownElement().updateComplete;
          await innerComposebox.updateComplete;

          assertEquals(
              0, innerComposebox.getDropdownElement().selectedMatchIndex);
          assertEquals('match 1', inputElement.value);
          assertEquals(0, innerComposebox.selectedMatchIndex);
        });

        test('clears dropdown matches after submitting a selected match',
            async () => {
              mockTimer.install();
              const TEST_QUERY = 'test query';
              const {app, innerComposebox} = parts;
              const inputElement = innerComposebox.getInputElement().$.input;
              assertTrue(isVisible(inputElement));

              simulateUserInput(inputElement, TEST_QUERY);
              mockTimer.tick(300);
              await mockSearchboxPageHandler.whenCalled(
                  'queryAutocompleteWithSuggestInventory');

              await setupAutocompleteResults(
                  searchboxCallbackRouterRemote, TEST_QUERY, mockTimer);
              while (!innerComposebox.getDropdownElement().result) {
                mockTimer.tick(10);
                await Promise.resolve();
              }

              const submitButton = getSubmitButton(innerComposebox);
              assertTrue(submitButton !== null);
              assertFalse(submitButton.disabled);

              mockSearchboxPageHandler.reset();
              pressEnter(inputElement);
              await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');
              mockTimer.tick(0);
              await innerComposebox.updateComplete;
              await app.updateComplete;

              assertEquals('', inputElement.value);
              assertEquals(
                  null, innerComposebox.getDropdownElement().result,
                  'Matches should be cleared after submit');

              // Pressing Enter again on the now-empty input is a no-op.
              mockSearchboxPageHandler.reset();
              pressEnter(inputElement);
              mockTimer.tick(0);
              await innerComposebox.updateComplete;
              assertFalse(inputElement.value.includes('\n'));
              assertEquals(
                  0, mockSearchboxPageHandler.getCallCount('submitQuery'));
              assertEquals(
                  0,
                  mockSearchboxPageHandler.getCallCount(
                      'openAutocompleteMatch'));
            });
      });
});

// =============================================================================
// Fork RESIZE/HEIGHT SUITE (fork path)
// The fork sets up the composebox-resize observers (the shared cr-composebox
// keeps them only for the Contextual Tasks Embedder) and renders the file
// carousel that fires carousel-resize. These checks target the fork; the legacy
// path's wrapper-side height flow is covered by the flag-off suite above.
// =============================================================================
suite(`ContextualTasksComposeboxResizeTest`, () => {
  // Minimal ResizeObserver stub whose instances can be triggered on demand;
  // the components debounce their resize callbacks, so tests advance the mock
  // timer after triggering.
  class MockResizeObserver {
    static instances: MockResizeObserver[] = [];

    constructor(private callback: ResizeObserverCallback) {
      MockResizeObserver.instances.push(this);
    }

    observe(_target: Element) {}
    unobserve(_target: Element) {}
    disconnect() {}

    trigger() {
      this.callback([], this);
    }
  }

  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
      ComposeboxPageHandlerRemote;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
      SearchboxPageHandlerRemote;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let mockTimer: MockTimer;
  let parts: CtComposeboxAppParts;

  setup(async () => {
    if (!window.chrome) {
      Object.assign(window, {chrome: {}});
    }
    if (!window.chrome.histograms) {
      Object.assign(window.chrome, {
        histograms: {
          recordEnumerationValue: () => {},
          recordUserAction: () => {},
          recordBoolean: () => {},
        },
      });
    }
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    window.ResizeObserver = MockResizeObserver;
    MockResizeObserver.instances = [];

    mockTimer = new MockTimer();

    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
      composeboxSmartTabSharingVisible: false,
      enableComposeboxJumpFix: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
      composeboxHintTextLensOverlay: 'Test Lens Hint',
      forcedEmbeddedPageHost: '',
      tabFaviconChipsToCoinsEnabled: false,
    });

    const testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    mockComposeboxPageHandler.setResultFor(
        'canShowNextboxAnimation', Promise.resolve({canShow: true}));
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    mockSearchboxPageHandler.setResultFor(
        'addTabContext', Promise.resolve({high: BigInt(1), low: BigInt(2)}));
    mockSearchboxPageHandler.setResultFor(
        'getInputState', Promise.resolve({state: new MockInputState()}));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler, searchboxCallbackRouter));

    parts = await createCtComposeboxApp(/* useFork= */ true);
    searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState());
    await microtasksFinished();

    assertTrue(
        MockResizeObserver.instances.length >= 1,
        'At least one ResizeObserver should be created');
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  test('inner composebox fires composebox-resize with its height', () => {
    mockTimer.install();
    const {innerComposebox} = parts;

    let firedHeight: number|undefined;
    innerComposebox.addEventListener('composebox-resize', (e: Event) => {
      const detail = (e as CustomEvent).detail;
      if (detail.height !== undefined) {
        firedHeight = detail.height;
      }
    });

    Object.defineProperty(innerComposebox, 'offsetHeight', {
      writable: true,
      configurable: true,
      value: 123,
    });

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    assertEquals(123, firedHeight);
  });

  test('carousel-resize from the fork carousel sets --carousel-height', () => {
    mockTimer.install();
    const {innerComposebox} = parts;
    const carousel = innerComposebox.shadowRoot.querySelector('#carousel');
    assertTrue(!!carousel, 'Fork should render the file carousel.');

    Object.defineProperty(carousel, 'clientHeight', {
      writable: true,
      configurable: true,
      value: 50,
    });

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    // The file carousel adds CAROUSEL_HEIGHT_PADDING (18) to clientHeight, and
    // the wrapper writes the result to --carousel-height on the inner element.
    assertEquals(
        '68px', innerComposebox.style.getPropertyValue('--carousel-height'));
  });
});
