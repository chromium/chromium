// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
/* clang-format off */
import {ComposeboxFile, GlifAnimationState} from 'chrome://resources/cr_components/composebox/common.js';
// <if expr="not is_android">
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
// </if>
/* clang-format on */
import {PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {SubmitButtonIconType} from 'chrome://resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxToolChipElement} from 'chrome://resources/cr_components/composebox/composebox_tool_chip.js';
import {VoiceSearchAction, VoiceSearchQuerySource} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import type {ContextualActionMenuElement} from 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState, VoiceSearchState} from 'chrome://resources/cr_components/search/constants.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteResult, PageRemote as SearchboxPageRemote, SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createCtComposeboxApp, fixtureUrl, getInputValue, getSubmitButton, simulateUserInput} from './contextual_tasks_test_utils.js';
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
      contextManagementInComposeboxEnabled: false,
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
        'canShowNextboxAnimation', Promise.resolve({canShow: true}));
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    // <if expr="not is_android">
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    // </if>
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
        mockComposeboxPageHandler, mockSearchboxPageHandler,
        searchboxCallbackRouter));

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

    assertEquals(1, mockSearchboxPageHandler.getCallCount('queryAutocomplete'));
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
            0, mockSearchboxPageHandler.getCallCount('queryAutocomplete'));
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
    await mockSearchboxPageHandler.whenCalled('queryAutocomplete');
    const calls = mockSearchboxPageHandler.getArgs('queryAutocomplete');
    const lastCall = calls[calls.length - 1];
    assertEquals('new query', lastCall[2]);
    assertEquals(SuggestInventory.kDefault, lastCall[5]);
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

  test('OnInputStateUpdateSetsStateAndCallsMojo', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;

    mockSearchboxPageHandler.reset();

    // Call onInputStateUpdate with ToolMode = 1 and ModelMode = 2.
    contextualComposebox.onInputStateUpdate(1, 2);

    const [toolMode] =
        await mockSearchboxPageHandler.whenCalled('setActiveToolMode');
    assertEquals(1, toolMode);
    const [, isSetByAim] =
        mockSearchboxPageHandler.getArgs('setActiveToolMode')[0];
    assertTrue(isSetByAim);

    const [modelMode] =
        await mockSearchboxPageHandler.whenCalled('setActiveModelMode');
    assertEquals(2, modelMode);
    const [, isModelSetByAim] =
        mockSearchboxPageHandler.getArgs('setActiveModelMode')[0];
    assertTrue(isModelSetByAim);
    // Verify that it is in tool mode.
    assertTrue(contextualComposebox.inToolModeForTesting);

    // Reset tool mode with `ToolMode.kUnspecified` (0),
    // and verify that it is reset.
    contextualComposebox.onInputStateUpdate(0, 0);
    assertFalse(contextualComposebox.inToolModeForTesting);
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
    assertEquals(1, mockSearchboxPageHandler.getCallCount('queryAutocomplete'));

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
    assertFalse(
        toolChipWithRc.classList.contains('unremovable'),
        'Canvas chip should be removable');

    // Verify chip can be removed by clicking.
    let eventFired = false;
    innerComposebox.addEventListener('tool-click', () => {
      eventFired = true;
    });

    getChip()!.click();
    await microtasksFinished();
    assertTrue(
        eventFired, 'Event should be fired when clicking chip to remove');

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
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tabInfo, null);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Wait for files to populate
    await innerComposebox.updateComplete;
    let files: ComposeboxFile[] =
        Array.from(innerComposebox.attachedContext.values());
    assertEquals(1, files.length);
    const initialFile = files[0]!;
    assertEquals('Initial Title', initialFile.name);
    assertEquals(1, initialFile.tabId);
    assertEquals('https://example.com', initialFile.url);

    // Suggest identical tab but updated title
    const updatedTabInfo = {
      ...tabInfo,
      title: 'Updated Title',
    };
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
        updatedTabInfo, null);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    files = Array.from(innerComposebox.attachedContext.values());
    assertEquals(1, files.length);
    const updatedFile = files[0]!;
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
        noUpdateTabInfo, null);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    files = Array.from(innerComposebox.attachedContext.values());
    assertEquals(1, files.length);
    // Reference should be exactly the same (no re-allocation or modification)
    assertEquals(updatedFile, files[0]);

    // Passing null deletes the auto-suggested tab file when not in side panel
    // OmniboxPageAction mode.
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(null, null);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await innerComposebox.updateComplete;
    assertEquals(0, innerComposebox.attachedContext.size);
  });

  test('OpeningMultipleNewThreadsPreservesAutoSuggestedTab', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;

    const tabInfo = {
      tabId: 1,
      title: 'Auto Tab',
      url: 'https://example.com',
      lastActive: {internalValue: BigInt(100)},
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
    };
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tabInfo, null);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    assertEquals(1, innerComposebox.files.size);
    assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());

    // Calling `clearInputAndFocus()` (what `onNewThreadClick_()` calls)
    // multiple times should preserve the auto-suggested tab.
    contextualTasksApp.$.composebox.clearInputAndFocus();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    assertEquals(1, innerComposebox.files.size);
    assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());

    contextualTasksApp.$.composebox.clearInputAndFocus();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    assertEquals(1, innerComposebox.files.size);
    assertTrue(innerComposebox.getHasAutomaticActiveTabChipToken());

    // Explicitly clearing all inputs removes the auto-suggested tab.
    innerComposebox.clearAllInputs(
        /* querySubmitted= */ false,
        /* shouldBlockAutoSuggestedTabs= */ true);
    await microtasksFinished();
    await innerComposebox.updateComplete;

    assertEquals(0, innerComposebox.files.size);
    assertFalse(innerComposebox.getHasAutomaticActiveTabChipToken());
  });

  test('SingleAutoTabFileDoesNotUpdatePlaceholder', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;
    const defaultApiHint =
        loadTimeData.getString('searchboxComposePlaceholder');
    innerComposebox.enableFileHint = true;

    const tabInfo = {
      tabId: 1,
      title: 'Auto Tab',
      url: 'https://example.com',
      lastActive: {internalValue: BigInt(100)},
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
    };
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tabInfo, null);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await innerComposebox.updateComplete;

    assertEquals(defaultApiHint, innerComposebox.inputPlaceholder);
  });

  suite('AutoSuggestedTabContextUploadMode', () => {
    const tabInfo = {
      tabId: 1,
      title: 'Tab 1',
      url: 'https://example.com/1',
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)},
    };

    setup(() => {
      composebox.isSidePanel = true;
    });

    test('DelayedUploadByDefault', async () => {
      loadTimeData.overrideValues({
        webUIOmniboxAskGAboutThisPageEnabled: false,
      });

      searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
          tabInfo, 'OmniboxPageAction');
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Verify addTabContext was called with delay_upload = true
      assertEquals(1, mockSearchboxPageHandler.getCallCount('addTabContext'));
      const args = mockSearchboxPageHandler.getArgs('addTabContext')[0];
      assertEquals(1, args[0]);  // tabId
      assertTrue(args[1]);       // delayUpload

      const innerComposebox = contextualTasksApp.$.composebox.$.composebox;
      await innerComposebox.updateComplete;
      assertEquals(1, innerComposebox.attachedContext.size);

      // Passing null deletes when feature flag is disabled.
      searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
          null, 'OmniboxPageAction');
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      await innerComposebox.updateComplete;
      assertEquals(0, innerComposebox.attachedContext.size);
    });

    test('ImmediateUploadWhenConditionsMet', async () => {
      loadTimeData.overrideValues({
        webUIOmniboxAskGAboutThisPageEnabled: true,
      });

      searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
          tabInfo, 'OmniboxPageAction');
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Verify addTabContext was called with delay_upload = false
      assertEquals(1, mockSearchboxPageHandler.getCallCount('addTabContext'));
      const args = mockSearchboxPageHandler.getArgs('addTabContext')[0];
      assertEquals(1, args[0]);  // tabId
      assertFalse(args[1]);      // delayUpload

      const innerComposebox = contextualTasksApp.$.composebox.$.composebox;
      await innerComposebox.updateComplete;
      assertEquals(1, innerComposebox.attachedContext.size);

      // Null should not delete when conditions are met.
      searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
          null, 'OmniboxPageAction');
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      await innerComposebox.updateComplete;
      assertEquals(1, innerComposebox.attachedContext.size);

      // Mismatched tab deletes it.
      const differentTab = {
        ...tabInfo,
        tabId: 2,
        title: 'Different Tab',
        url: 'https://different.com',
      };
      searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
          differentTab, 'OmniboxPageAction');
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
      await innerComposebox.updateComplete;
      assertEquals(0, innerComposebox.attachedContext.size);
    });

    test('DelayedUploadWhenNotPageAction', async () => {
      loadTimeData.overrideValues({
        webUIOmniboxAskGAboutThisPageEnabled: true,
      });

      searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
          tabInfo, 'AppMenu');
      await searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Verify addTabContext was called with delay_upload = true
      assertEquals(1, mockSearchboxPageHandler.getCallCount('addTabContext'));
      const args = mockSearchboxPageHandler.getArgs('addTabContext')[0];
      assertEquals(1, args[0]);  // tabId
      assertTrue(args[1]);       // delayUpload
    });
  });
});

// =============================================================================
// Fork DUAL-PATH SMOKE SUITE
// Infrastructure-only coverage: verifies the wrapper's
// `useContextualTasksComposeboxFork` ternary picks the right inner element
// and that wrapper-teplate bindings reach the inner element at mount, on both
// paths. The fork is a smoke skeleton, so nothing here may depend on
// fork-specific inner composebox behavior.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkSmokeTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
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
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          // <if expr="not is_android">
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          // </if>
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
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

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

        test('inner composebox does not query zps on initial mount', () => {
          const {innerComposebox} = parts;
          // The wrapper template binds `.queryZpsOnLoad="${false}"`, so the
          // mount in setup() must not blindly query zps in connectedCallback.
          assertFalse(innerComposebox.queryZpsOnLoad);
          assertEquals(
              0, mockSearchboxPageHandler.getCallCount('queryAutocomplete'));
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
// yet (selected-match submit, dropdown/result-changed, files, ...) stay
// in the flag-off suites above.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkBasicInputTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
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
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          // <if expr="not is_android">
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          // </if>
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
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
        });

        test('EnterKeyOnEmptyInputDoesNotAddNewLineOrSubmit', async () => {
          const {innerComposebox} = parts;
          const inputElement = innerComposebox.getInputElement().$.input;
          const keydownDiv =
              innerComposebox.shadowRoot.querySelector<HTMLElement>(
                  '#composebox');
          assertTrue(keydownDiv !== null);

          assertEquals('', getInputValue(inputElement));
          mockSearchboxPageHandler.reset();

          // Action: Press Enter on empty input.
          pressEnter(keydownDiv);
          await microtasksFinished();

          // Assert: No newline and no submission.
          assertFalse(getInputValue(inputElement).includes('\n'));
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
              assertEquals('', getInputValue(inputElement));
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

          const initialPlaceholder = inputElement.getAttribute('placeholder');

          // Set to true.
          wrapper.isOverlayOpenForAimVisualSearch = true;
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;

          assertTrue(wrapper.isOverlayOpenForAimVisualSearch);
          assertEquals(
              'Test Lens Hint', innerComposebox.inputPlaceholderOverride);
          assertEquals(
              'Test Lens Hint', inputElement.getAttribute('placeholder'));

          // Set back to false.
          wrapper.isOverlayOpenForAimVisualSearch = false;
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;

          assertFalse(wrapper.isOverlayOpenForAimVisualSearch);
          assertEquals('', innerComposebox.inputPlaceholderOverride);
          assertEquals(
              initialPlaceholder, inputElement.getAttribute('placeholder'));
        });

        test('lens search tooltip showing reflects attribute', async () => {
          const {wrapper} = parts;

          assertFalse(wrapper.isLensSearchTooltipShowing);
          assertFalse(wrapper.hasAttribute('is-lens-search-tooltip-showing'));

          wrapper.isLensSearchTooltipShowing = true;
          await wrapper.updateComplete;

          assertTrue(wrapper.isLensSearchTooltipShowing);
          assertTrue(wrapper.hasAttribute('is-lens-search-tooltip-showing'));

          wrapper.isLensSearchTooltipShowing = false;
          await wrapper.updateComplete;

          assertFalse(wrapper.isLensSearchTooltipShowing);
          assertFalse(wrapper.hasAttribute('is-lens-search-tooltip-showing'));
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

          wrapper.isZeroState = true;
          simulateUserInput(innerComposebox.getInputElement().$.input, '');

          innerComposebox.clearAutocompleteMatches = () => {
            clearAutocompleteMatchesCallCount++;
          };

          innerComposebox.queryAutocomplete = () => {
            queryAutocompleteCallCount++;
          };

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

        test('clear-smart-compose event clears the inline hint', () => {
          const {innerComposebox} = parts;
          innerComposebox.smartComposeInlineHint = 'test hint';
          innerComposebox.getInputElement().dispatchEvent(
              new CustomEvent('clear-smart-compose'));
          assertEquals('', innerComposebox.smartComposeInlineHint);
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
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          // <if expr="not is_android">
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          // </if>
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
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

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
              await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

              const whenResultChanged =
                  eventToPromise<CustomEvent<AutocompleteResult>>(
                      'result-changed', innerComposebox);
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    queryId: parts.innerComposebox.activeQueryId,
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
              await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

              let fired = false;
              innerComposebox.addEventListener(
                  'result-changed', () => fired = true);

              // The response input does not match the last queried input.
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    queryId: parts.innerComposebox.activeQueryId + 1,
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
                    queryId: parts.innerComposebox.activeQueryId,
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
                    queryId: parts.innerComposebox.activeQueryId,
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
                    queryId: parts.innerComposebox.activeQueryId,
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
                queryId: parts.innerComposebox.activeQueryId,
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
          assertEquals('match 1', getInputValue(inputElement));
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
              await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

              await setupAutocompleteResults(
                  searchboxCallbackRouterRemote, innerComposebox.activeQueryId,
                  TEST_QUERY, mockTimer);
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

              assertEquals('', getInputValue(inputElement));
              assertEquals(
                  null, innerComposebox.getDropdownElement().result,
                  'Matches should be cleared after submit');

              // Pressing Enter again on the now-empty input is a no-op.
              mockSearchboxPageHandler.reset();
              pressEnter(inputElement);
              mockTimer.tick(0);
              await innerComposebox.updateComplete;
              assertFalse(getInputValue(inputElement).includes('\n'));
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
// Fork DUAL-PATH CONTEXT MENU / SMART TAB SHARING SUITE
// The fork renders the contextual entrypoint-and-menu and mirrors the legacy
// <cr-composebox>'s Smart Tab Sharing behavior (initial-active fetch on
// connect, clearing shared tab context when sharing turns active), so these
// tests run on both paths. The app's help-bubble wiring and the onboarding
// tooltip resolve the menu through the element chain asserted here.
// =============================================================================
// `useFork` toggles `useContextualTasksComposeboxFork`:
// - true: uses the contextual tasks inner composebox (routing calls via `mockComposeboxPageHandler`).
// - false: uses the legacy <cr-composebox> (routing calls via `mockSearchboxPageHandler`).
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkContextMenuTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
        let testProxy: TestContextualTasksBrowserProxy;
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let parts: CtComposeboxAppParts;

        // Both inner elements initialize `smartTabSharingVisible` from
        // loadTimeData, so the override must be applied before the mount.
        async function mountApp(smartTabSharingVisible: boolean) {
          loadTimeData.overrideValues(
              {composeboxSmartTabSharingVisible: smartTabSharingVisible});
          parts = await createCtComposeboxApp(useFork);
        }

        function getEntrypointAndMenu() {
          const entrypoint = parts.innerComposebox.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypoint);
          return entrypoint;
        }

        setup(() => {
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
            contextManagementInComposeboxEnabled: false,
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
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          // Smart Tab Sharing is a desktop-only feature ([EnableIfNot=is_android] in mojom).
          // <if expr="not is_android">
          mockSearchboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          // </if>
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
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));
        });

        test('renders the contextual entrypoint and exposes it', async () => {
          await mountApp(/*smartTabSharingVisible=*/ false);
          const entrypoint = getEntrypointAndMenu();
          assertEquals('contextEntrypoint', entrypoint.id);
          assertEquals(
              entrypoint, parts.innerComposebox.getContextEntrypointElement());
        });

        test('app query chain reaches the contextual action menu', async () => {
          await mountApp(/*smartTabSharingVisible=*/ false);
          // Mirrors the app's help-bubble lookup:
          // wrapper -> #composebox -> #contextEntrypoint -> #menu.
          const wrapper =
              parts.app.shadowRoot.querySelector('contextual-tasks-composebox');
          assertTrue(!!wrapper);
          const innerComposebox =
              wrapper.shadowRoot.querySelector('#composebox');
          assertTrue(!!innerComposebox);
          const innerShadowRoot = innerComposebox.shadowRoot;
          assertTrue(!!innerShadowRoot);
          const entrypoint =
              innerShadowRoot.querySelector('#contextEntrypoint');
          assertTrue(!!entrypoint);
          const entrypointShadowRoot = entrypoint.shadowRoot;
          assertTrue(!!entrypointShadowRoot);
          const menu = entrypointShadowRoot.querySelector('#menu');
          assertTrue(!!menu);
          assertEquals('CR-COMPOSEBOX-CONTEXTUAL-ACTION-MENU', menu.tagName);
        });

        test(
            'forwards contextManagementInComposeboxEnabled to the menu',
            async () => {
              loadTimeData.overrideValues(
                  {contextManagementInComposeboxEnabled: true});
              await mountApp(/*smartTabSharingVisible=*/ false);
              const entrypointAndMenu = getEntrypointAndMenu();
              await entrypointAndMenu.updateComplete;
              assertTrue(
                  entrypointAndMenu.contextManagementInComposeboxEnabled);

              const menu =
                  entrypointAndMenu.shadowRoot
                      .querySelector<ContextualActionMenuElement>('#menu');
              assertTrue(!!menu);
              await menu.updateComplete;
              assertTrue(menu.contextManagementInComposeboxEnabled);
            });

        test(
            'zero state drives glif animation into the entrypoint button',
            async () => {
              // Deterministic animation-limiting path: the wrapper must consult
              // canShowNextboxAnimation() before starting the glif animation
              // on a zero-state transition.
              loadTimeData.overrideValues(
                  {contextMenuAnimationLimitingEnabled: true});
              mockComposeboxPageHandler.setResultFor(
                  'canShowNextboxAnimation', Promise.resolve({canShow: true}));
              await mountApp(/*smartTabSharingVisible=*/ false);
              const {app, wrapper, innerComposebox} = parts;
              const entrypointAndMenu = getEntrypointAndMenu();
              const button = entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-entrypoint-button');
              assertTrue(!!button);
              const entrypointButton = button;

              async function settleChain() {
                await microtasksFinished();
                await app.updateComplete;
                await wrapper.updateComplete;
                await innerComposebox.updateComplete;
                await entrypointAndMenu.updateComplete;
                await entrypointButton.updateComplete;
              }

              // Baseline: leaving zero state marks the animation ineligible.
              testProxy.callbackRouterRemote.onZeroStateChange(false);
              await testProxy.callbackRouterRemote.$.flushForTesting();
              await settleChain();
              assertEquals(
                  GlifAnimationState.INELIGIBLE,
                  entrypointButton.glifAnimationState);
              assertEquals(
                  null,
                  entrypointButton.shadowRoot.querySelector('#glowWrapper'));

              // Entering zero state drives the wrapper's glifAnimationState_
              // through the real chain: app -> wrapper -> innerComposebox ->
              // entrypointAndMenu -> entrypointButton.
              mockComposeboxPageHandler.resetResolver(
                  'canShowNextboxAnimation');
              mockComposeboxPageHandler.setResultFor(
                  'canShowNextboxAnimation', Promise.resolve({canShow: true}));
              testProxy.callbackRouterRemote.onZeroStateChange(true);
              await testProxy.callbackRouterRemote.$.flushForTesting();
              await settleChain();
              assertEquals(
                  1,
                  mockComposeboxPageHandler.getCallCount(
                      'canShowNextboxAnimation'));
              assertEquals(
                  GlifAnimationState.STARTED,
                  entrypointButton.glifAnimationState);
              assertEquals(
                  'started',
                  entrypointButton.getAttribute('glif-animation-state'));
              const glowWrapper =
                  entrypointButton.shadowRoot.querySelector('#glowWrapper');
              assertTrue(!!glowWrapper);
              assertTrue(glowWrapper.classList.contains('glow-container'));

              // Leaving zero state retracts the glow render contract.
              testProxy.callbackRouterRemote.onZeroStateChange(false);
              await testProxy.callbackRouterRemote.$.flushForTesting();
              await settleChain();
              assertEquals(
                  GlifAnimationState.INELIGIBLE,
                  entrypointButton.glifAnimationState);
              assertEquals(
                  'ineligible',
                  entrypointButton.getAttribute('glif-animation-state'));
              assertEquals(
                  null,
                  entrypointButton.shadowRoot.querySelector('#glowWrapper'));
              assertEquals(
                  null,
                  entrypointButton.shadowRoot.querySelector('.glow-container'));
            });

        // Smart Tab Sharing tests are desktop-only ([EnableIfNot=is_android] in mojom).
        // <if expr="not is_android">
        test(
            'fetches Smart Tab Sharing active state on connect when visible',
            async () => {
              mockSearchboxPageHandler.setResultFor(
                  'getSmartTabSharingActive', Promise.resolve({active: true}));
              await mountApp(/*smartTabSharingVisible=*/ true);
              assertEquals(
                  1,
                  mockSearchboxPageHandler.getCallCount(
                      'getSmartTabSharingActive'));
              assertEquals(
                  0,
                  mockComposeboxPageHandler.getCallCount(
                      'getSmartTabSharingActive'));
              await microtasksFinished();
              assertTrue(parts.innerComposebox.smartTabSharingActive);
            });

        test(
            'does not fetch Smart Tab Sharing active state when not visible',
            async () => {
              await mountApp(/*smartTabSharingVisible=*/ false);
              assertEquals(
                  0,
                  mockSearchboxPageHandler.getCallCount(
                      'getSmartTabSharingActive'));
              assertEquals(
                  0,
                  mockComposeboxPageHandler.getCallCount(
                      'getSmartTabSharingActive'));
            });

        test('SmartTabSharingActiveChangedFiresMojo', async () => {
          await mountApp(/*smartTabSharingVisible=*/ false);
          const entrypointAndMenu = getEntrypointAndMenu();

          mockSearchboxPageHandler.reset();

          entrypointAndMenu.dispatchEvent(
              new CustomEvent('smart-tab-sharing-active-changed', {
                detail: {active: true},
                bubbles: true,
                composed: true,
              }));

          const activeArg = await mockSearchboxPageHandler.whenCalled(
              'setSmartTabSharingActive');
          assertEquals(
              1,
              mockSearchboxPageHandler.getCallCount(
                  'setSmartTabSharingActive'));
          assertEquals(true, activeArg);
        });
        // </if>

        test('ContextMenuOpenedFiresMojo', async () => {
          await mountApp(/*smartTabSharingVisible=*/ false);
          const entrypointAndMenu = getEntrypointAndMenu();

          mockComposeboxPageHandler.reset();

          entrypointAndMenu.dispatchEvent(
              new CustomEvent('context-menu-opened', {
                bubbles: true,
                composed: true,
              }));

          await mockComposeboxPageHandler.whenCalled('onContextMenuOpened');
          assertEquals(
              1, mockComposeboxPageHandler.getCallCount('onContextMenuOpened'));
        });

        // Smart Tab Sharing is desktop-only ([EnableIfNot=is_android] in mojom).
        // <if expr="not is_android">
        test(
            'clears shared tab context when sharing becomes active',
            async () => {
              await mountApp(/*smartTabSharingVisible=*/ false);
              const {innerComposebox} = parts;
              const entrypointAndMenu = getEntrypointAndMenu();

              entrypointAndMenu.dispatchEvent(
                  new CustomEvent('add-tab-context', {
                    detail: {
                      id: 1,
                      title: 'Shared tab',
                      // WebUI maps Mojo URL to a string, which is passed to
                      // `new URL()`.
                      url: 'https://example.com/',
                      delayUpload: false,
                      origin: TabUploadOrigin.CURRENT_TAB_CHIP,
                    },
                    bubbles: true,
                    composed: true,
                  }));
              await mockSearchboxPageHandler.whenCalled('addTabContext');
              await microtasksFinished();
              assertEquals(1, innerComposebox.attachedContext.size);

              entrypointAndMenu.dispatchEvent(
                  new CustomEvent('smart-tab-sharing-active-changed', {
                    detail: {active: true},
                    bubbles: true,
                    composed: true,
                  }));
              const activeArg = await mockSearchboxPageHandler.whenCalled(
                  'setSmartTabSharingActive');
              assertEquals(true, activeArg);
              await microtasksFinished();
              assertEquals(0, innerComposebox.attachedContext.size);
            });
        // </if>
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

    observedTargets: Element[] = [];

    constructor(private callback: ResizeObserverCallback) {
      MockResizeObserver.instances.push(this);
    }

    observe(target: Element) {
      this.observedTargets.push(target);
    }
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
        'canShowNextboxAnimation', Promise.resolve({canShow: true}));
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    // <if expr="not is_android">
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    // </if>
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
        mockComposeboxPageHandler, mockSearchboxPageHandler,
        searchboxCallbackRouter));

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

  test(
      'carousel-resize from the fork carousel sets --carousel-height',
      async () => {
        const {innerComposebox} = parts;
        // The carousel is only rendered while files are present.
        const token = {high: 0n, low: 1n} as unknown as UnguessableToken;
        innerComposebox.addFileContextForTesting(new ComposeboxFile(
            token, 'unknown.dat', 'unknown/type', InputType.kLensFile));
        await innerComposebox.updateComplete;

        const carousel = innerComposebox.shadowRoot.querySelector('#carousel');
        assertTrue(!!carousel, 'Fork should render the file carousel.');

        mockTimer.install();
        Object.defineProperty(carousel, 'clientHeight', {
          writable: true,
          configurable: true,
          value: 50,
        });

        const carouselObservers = MockResizeObserver.instances.filter(
            obs => obs.observedTargets.includes(carousel));
        assertTrue(carouselObservers.length > 0);
        carouselObservers.forEach(obs => obs.trigger());
        mockTimer.tick(100);

        // The carousel adds CAROUSEL_HEIGHT_PADDING (18) to clientHeight and
        // the wrapper writes the result to --carousel-height.
        assertEquals(
            '68px',
            innerComposebox.style.getPropertyValue('--carousel-height'));
      });
});

// =============================================================================
// Fork GLOW RENDER SURFACE SUITE (both paths)
// The wrapper drives animation and energy-effect state on whichever inner
// element the flag selects, so both templates must render the same
// `search-animated-glow` consumer; the legacy path doubles as the baseline for
// the fork's render and state-propagation contract.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkGlowTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
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
            energyEffectEnabled: true,
          });

          const testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
          BrowserProxyImpl.setInstance(testProxy);

          const mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          const mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          // <if expr="not is_android">
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          // </if>
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
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
        });

        test('both paths render the glow render surface', () => {
          const {innerComposebox} = parts;
          const glow =
              innerComposebox.shadowRoot.querySelector('search-animated-glow');
          assertTrue(!!glow, 'search-animated-glow should be in the DOM');
          assertEquals('animatedSearchElement', glow.id);
          const exportparts = glow.getAttribute('exportparts');
          assertTrue(
              !!exportparts && exportparts.includes('composebox-background'),
              'Glow should re-export the composebox-background part');
        });

        test('wrapper startExpandAnimation reaches the glow', async () => {
          const {wrapper, innerComposebox} = parts;
          const glow =
              innerComposebox.shadowRoot.querySelector('search-animated-glow');
          assertTrue(!!glow);
          await wrapper.startExpandAnimation();
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;
          await glow.updateComplete;
          assertEquals(GlowAnimationState.EXPANDING, glow.animationState);
        });

        test('energy state reaches the inner host and the glow', async () => {
          const {wrapper, innerComposebox} = parts;
          await wrapper.updateComplete;
          await innerComposebox.updateComplete;
          assertTrue(innerComposebox.energyEffectEnabled);
          assertTrue(innerComposebox.hasAttribute('energy-effect-enabled'));
          const glow =
              innerComposebox.shadowRoot.querySelector('search-animated-glow');
          assertTrue(!!glow);
          await glow.updateComplete;
          assertTrue(glow.energyEffectAnimationEnabled);
        });
      });
});

[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkErrorScrimTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let searchboxCallbackRouterRemote: SearchboxPageRemote;
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

          const testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
          BrowserProxyImpl.setInstance(testProxy);

          mockComposeboxPageHandler =
              TestMock.fromClass(ComposeboxPageHandlerRemote);
          mockComposeboxPageHandler.setResultFor(
              'canShowNextboxAnimation', Promise.resolve({canShow: true}));
          mockSearchboxPageHandler =
              TestMock.fromClass(SearchboxPageHandlerRemote);
          // <if expr="not is_android">
          mockComposeboxPageHandler.setResultFor(
              'getSmartTabSharingActive', Promise.resolve({active: false}));
          // </if>
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
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
          searchboxCallbackRouterRemote.onInputStateChanged(
              new MockInputState());
          await microtasksFinished();
        });

        test('error scrim lifecycle: render, inert, dismiss', async () => {
          const {innerComposebox} = parts;
          const composeboxDiv =
              innerComposebox.shadowRoot.querySelector('#composebox');
          assertTrue(!!composeboxDiv);

          // No error: no scrim in the DOM, composebox stays interactive.
          assertFalse(
              !!innerComposebox.shadowRoot.querySelector('ntp-error-scrim'));
          assertFalse(composeboxDiv.hasAttribute('inert'));

          innerComposebox.errorMessage = 'Upload failed';
          await innerComposebox.updateComplete;

          const errorScrim =
              innerComposebox.shadowRoot.querySelector('ntp-error-scrim');
          assertTrue(!!errorScrim);
          await errorScrim.updateComplete;
          assertEquals('Upload failed', errorScrim.errorMessage);
          const errorMessageElement =
              errorScrim.shadowRoot.querySelector('#errorMessage');
          assertTrue(!!errorMessageElement);
          assertEquals('Upload failed', errorMessageElement.textContent);
          assertTrue(composeboxDiv.hasAttribute('inert'));
          // Outside voice search mode the scrim host must stay static, as in
          // the shared template (no fork-specific positioning).
          assertEquals('static', window.getComputedStyle(errorScrim).position);

          const dismissErrorButton =
              errorScrim.shadowRoot.querySelector<HTMLElement>(
                  '#dismissErrorButton');
          assertTrue(!!dismissErrorButton);
          dismissErrorButton.click();
          await microtasksFinished();
          await innerComposebox.updateComplete;

          assertEquals('', innerComposebox.errorMessage);
          assertFalse(
              !!innerComposebox.shadowRoot.querySelector('ntp-error-scrim'));
          assertFalse(composeboxDiv.hasAttribute('inert'));
        });

        test(
            'error scrim is position absolute in voice search mode',
            async () => {
              const {innerComposebox} = parts;
              innerComposebox.inVoiceSearchMode = true;
              innerComposebox.errorMessage = 'Network error';
              await innerComposebox.updateComplete;

              const errorScrim =
                  innerComposebox.shadowRoot.querySelector('ntp-error-scrim');
              assertTrue(!!errorScrim);
              await errorScrim.updateComplete;
              assertEquals(
                  'absolute', window.getComputedStyle(errorScrim).position);

              innerComposebox.inVoiceSearchMode = false;
              innerComposebox.errorMessage = '';
              await innerComposebox.updateComplete;
            });
      });
});

// =============================================================================
// Fork DUAL-PATH VOICE SUITE
// Voice search is implemented by both the legacy <cr-composebox> and the
// <contextual-tasks-inner-composebox>, so these tests run on both paths, in two
// arms: voice search coherence disabled and enabled. All flows go through the
// real component chain (mock SpeechRecognition callbacks, real buttons);
// `composebox-voice-search-*` events are never dispatched directly.
// =============================================================================

// Deferred-end SpeechRecognition fake: the real API fires `onend`
// asynchronously after abort()/stop(), while production cleanup calls abort()
// before resetting element state. A synchronous `onend` would hit the error
// branches of `onEnd_`, so tests flush the single pending end explicitly.
class FakeSpeechRecognition {
  onresult: ((e: SpeechRecognitionEvent) => void)|null = null;
  onend: (() => void)|null = null;
  onerror: ((e: SpeechRecognitionErrorEvent) => void)|null = null;
  onnomatch: (() => void)|null = null;
  onaudiostart: (() => void)|null = null;
  onspeechstart: (() => void)|null = null;
  interimResults: boolean = false;
  continuous: boolean = false;
  lang: string = '';
  started: boolean = false;
  private pendingEnd_: boolean = false;

  start() {
    this.started = true;
  }

  stop() {
    this.started = false;
    this.pendingEnd_ = true;
  }

  abort() {
    this.started = false;
    this.pendingEnd_ = true;
  }

  flushEnd() {
    if (!this.pendingEnd_) {
      return;
    }
    this.pendingEnd_ = false;
    if (this.onend) {
      this.onend();
    }
  }
}

function createVoiceResults(transcripts: string[]): SpeechRecognitionEvent {
  return {
    results: transcripts.map(transcript => ({
                              isFinal: false,
                              length: 1,
                              0: {transcript, confidence: 1},
                            })),
    resultIndex: 0,
  } as unknown as SpeechRecognitionEvent;
}

[true, false].forEach(useFork => {
  [true, false].forEach(coherenceEnabled => {
    suite(
        `ContextualTasksComposeboxForkVoiceTest ` +
            `(useContextualTasksComposeboxFork = ${useFork}, ` +
            `coherence = ${coherenceEnabled})`,
        () => {
          let testProxy: TestContextualTasksBrowserProxy;
          let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
              ComposeboxPageHandlerRemote;
          let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
              SearchboxPageHandlerRemote;
          let searchboxCallbackRouterRemote: SearchboxPageRemote;
          let windowProxy: TestMock<WindowProxy>& WindowProxy;
          let mockRecognition: FakeSpeechRecognition|null;
          let metrics: MetricsTracker;
          let parts: CtComposeboxAppParts;

          setup(async () => {
            const voiceSearchClass = window.customElements.get('cr-composebox-voice-search') as any;
            if (voiceSearchClass) {
              voiceSearchClass.activeRecognition_ = null;
              voiceSearchClass.pendingStartInstance_ = null;
            }
            if (!window.chrome) {
              Object.assign(window, {chrome: {}});
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
              voiceSearchCoherenceComposeboxesEnabled: coherenceEnabled,
              voiceSearchCoherenceCobrowsingComposeboxEnabled: coherenceEnabled,
              isSystemVoiceSearchEnabled: false,
            });

            metrics = fakeMetricsPrivate();

            testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
            BrowserProxyImpl.setInstance(testProxy);

            mockRecognition = null;
            windowProxy = TestMock.fromClass(WindowProxy);
            windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
            windowProxy.setResultMapperFor('createSpeechRecognition', () => {
              mockRecognition = new FakeSpeechRecognition();
              return mockRecognition as unknown as SpeechRecognition;
            });
            windowProxy.setResultMapperFor(
                'matchMedia', (query: string) => window.matchMedia(query));
            windowProxy.setResultFor('setTimeout', 0);
            WindowProxy.setInstance(windowProxy);

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
                'getInputState',
                Promise.resolve({state: new MockInputState()}));
            const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
            searchboxCallbackRouterRemote =
                searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
            ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
                mockComposeboxPageHandler, mockSearchboxPageHandler,
                searchboxCallbackRouter));

            parts = await createCtComposeboxApp(useFork);
            searchboxCallbackRouterRemote.onInputStateChanged(
                new MockInputState());
            await microtasksFinished();
          });

          teardown(() => {
            // Release the static SpeechRecognition coordination state while
            // the element is still in the DOM, then flush the deferred end.
            const voiceSearch = parts.innerComposebox.shadowRoot.querySelector(
                'cr-composebox-voice-search');
            if (voiceSearch) {
              (voiceSearch as unknown as {voiceModeEndCleanup_: () => void})
                  .voiceModeEndCleanup_();
            }
            if (mockRecognition) {
              mockRecognition.flushEnd();
            }
          });

          function getVoiceSearch() {
            const voiceSearch = parts.innerComposebox.shadowRoot.querySelector(
                'cr-composebox-voice-search');
            assertTrue(!!voiceSearch, 'Voice search element should exist');
            return voiceSearch;
          }

          function getRecognition(): FakeSpeechRecognition {
            const recognition = mockRecognition;
            assertTrue(!!recognition, 'Speech recognition should be created');
            return recognition;
          }

          function getAnimatedGlow() {
            const animatedGlow = parts.innerComposebox.shadowRoot.querySelector(
                'search-animated-glow');
            assertTrue(!!animatedGlow, 'Animated glow should exist');
            return animatedGlow;
          }

          async function enterVoiceSearchMode() {
            const voiceSearchButton =
                parts.innerComposebox.shadowRoot.querySelector<HTMLElement>(
                    '#voiceSearchButton');
            assertTrue(!!voiceSearchButton, 'Voice search button should exist');
            voiceSearchButton.click();
            await microtasksFinished();
            await parts.innerComposebox.updateComplete;
            await getAnimatedGlow().updateComplete;
            await getVoiceSearch().updateComplete;
          }

          function fireRecognitionResult(transcripts: string[]) {
            const recognition = getRecognition();
            assertTrue(!!recognition.onresult, 'onresult should be wired');
            recognition.onresult(createVoiceResults(transcripts));
          }

          function fireRecognitionError(error: string) {
            const recognition = getRecognition();
            assertTrue(!!recognition.onerror, 'onerror should be wired');
            recognition.onerror({error} as SpeechRecognitionErrorEvent);
          }

          async function whenMetricSourceFetched() {
            await mockSearchboxPageHandler.whenCalled('getPageClassification');
            await microtasksFinished();
            assertEquals(
                1,
                mockSearchboxPageHandler.getCallCount('getPageClassification'));
            assertEquals(
                'CO_BROWSING_COMPOSEBOX', getVoiceSearch().metricSource);
          }

          // Executes the last idle-timeout callback scheduled after `baseline`
          // setTimeout calls. The call-count baseline excludes timers already
          // armed during start; the delay filter excludes the 0ms
          // outside-listener registration.
          async function fireIdleTimeout(baseline: number) {
            const idleTimeout = getVoiceSearch().idleTimeout;
            const idleCallbacks =
                windowProxy.getArgs('setTimeout')
                    .slice(baseline)
                    .filter(
                        (args: [() => void, number]) => args[1] === idleTimeout)
                    .map((args: [() => void, number]) => args[0]);
            const lastIdleCallback = idleCallbacks.pop();
            assertTrue(
                !!lastIdleCallback, 'onresult should re-arm the idle timer');
            lastIdleCallback();
            await microtasksFinished();
            await parts.innerComposebox.updateComplete;
          }

          suite('SurfaceAndStartup', () => {
            test(
                'voice button and overlay render behind inherited gating',
                async () => {
                  const inner = parts.innerComposebox;
                  const voiceSearchButton =
                      inner.shadowRoot.querySelector<HTMLElement>(
                          '#voiceSearchButton');
                  assertTrue(!!voiceSearchButton);
                  assertTrue(voiceSearchButton.part.contains('voice-icon'));
                  assertEquals(
                      'cr:mic-filled',
                      voiceSearchButton.getAttribute('iron-icon'));
                  assertEquals(
                      loadTimeData.getString('voiceSearchButtonLabel'),
                      voiceSearchButton.title);
                  assertTrue(!!inner.shadowRoot.querySelector(
                      'cr-composebox-voice-search'));

                  inner.showVoiceSearch = false;
                  await inner.updateComplete;
                  assertFalse(
                      !!inner.shadowRoot.querySelector('#voiceSearchButton'));
                  assertFalse(!!inner.shadowRoot.querySelector(
                      'cr-composebox-voice-search'));
                });

            test(
                'voice surface absent without speech recognition API',
                async () => {
                  windowProxy.setResultFor('hasWebkitSpeechRecognition', false);
                  document.body.innerHTML = window.trustedTypes!.emptyHTML;
                  parts = await createCtComposeboxApp(useFork);

                  const inner = parts.innerComposebox;
                  assertTrue(inner.showVoiceSearch);
                  assertFalse(
                      !!inner.shadowRoot.querySelector('#voiceSearchButton'));
                  assertFalse(!!inner.shadowRoot.querySelector(
                      'cr-composebox-voice-search'));
                });

            test(
                'entering voice search mode hides composebox and starts ' +
                    'recognition',
                async () => {
                  const inner = parts.innerComposebox;
                  const voiceSearch = getVoiceSearch();
                  const composeboxDiv =
                      inner.shadowRoot.querySelector('#composebox');
                  assertTrue(!!composeboxDiv);
                  assertEquals(
                      'none', window.getComputedStyle(voiceSearch).display);

                  await enterVoiceSearchMode();

                  assertTrue(getRecognition().started);
                  assertTrue(inner.inVoiceSearchMode);
                  assertTrue(
                      parts.wrapper.hasAttribute('in-voice-search-mode_'));
                  assertEquals(
                      'none', window.getComputedStyle(composeboxDiv).display);
                  assertEquals(
                      'block', window.getComputedStyle(voiceSearch).display);
                  assertTrue(getAnimatedGlow().isListening);
                  assertEquals(
                      1,
                      metrics.count(
                          'ContextualTasks.VoiceSearch.StateV2',
                          VoiceSearchState.VOICE_SEARCH_BUTTON_CLICKED));
                });

            test('glow receives voice bindings', async () => {
              const animatedGlow = getAnimatedGlow();
              assertEquals(
                  coherenceEnabled,
                  animatedGlow.coloredTicTacVoiceAnimationEnabled);
              assertTrue(animatedGlow.requiresVoice);
              assertFalse(animatedGlow.isListening);

              await enterVoiceSearchMode();
              assertTrue(animatedGlow.isListening);

              if (coherenceEnabled) {
                assertTrue(
                    !!animatedGlow.shadowRoot.querySelector('#recordingWave'));
                assertFalse(
                    !!animatedGlow.shadowRoot.querySelector('audio-wave'));
              } else {
                fireRecognitionResult(['hello', 'world']);
                await microtasksFinished();
                await parts.innerComposebox.updateComplete;
                await animatedGlow.updateComplete;

                assertEquals('helloworld', animatedGlow.transcript);
                assertTrue(animatedGlow.receivedSpeech);
                const audioWave =
                    animatedGlow.shadowRoot.querySelector<HTMLElement&{
                      transcript: string,
                      receivedSpeech: boolean,
                    }>('audio-wave');
                assertTrue(!!audioWave);
                assertEquals('helloworld', audioWave.transcript);
                assertTrue(audioWave.receivedSpeech);
                assertFalse(
                    !!animatedGlow.shadowRoot.querySelector('#recordingWave'));
              }
            });
          });

          suite('RecognitionAndSubmission', () => {
            test('transcription success submits voice query', async () => {
              const inner = parts.innerComposebox;
              await whenMetricSourceFetched();

              await enterVoiceSearchMode();
              assertEquals(
                  1,
                  metrics.count(
                      'VoiceSearch.Action.CO_BROWSING_COMPOSEBOX',
                      VoiceSearchAction.ACTIVATED_BY_ICON));

              const baseline = windowProxy.getCallCount('setTimeout');
              fireRecognitionResult(['hello', 'world']);
              await microtasksFinished();
              await fireIdleTimeout(baseline);

              const submitArgs =
                  await mockSearchboxPageHandler.whenCalled('submitQuery');
              assertEquals(
                  1, mockSearchboxPageHandler.getCallCount('submitQuery'));
              assertEquals('helloworld', submitArgs[0]);
              assertTrue(submitArgs[6], 'Should submit as a voice query');
              assertFalse(inner.inVoiceSearchMode);
              assertEquals(
                  1,
                  metrics.count(
                      'ContextualTasks.VoiceSearch.StateV2',
                      VoiceSearchState.SUCCESSFUL_TRANSCRIPT));
              assertEquals(
                  1,
                  metrics.count(
                      'VoiceSearch.Action.CO_BROWSING_COMPOSEBOX',
                      VoiceSearchAction.QUERY_SUBMITTED));
              assertEquals(
                  1,
                  metrics.count(
                      'VoiceSearch.QuerySubmission.Source',
                      VoiceSearchQuerySource.NEXTBOX_COMPOSEBOX));

              getRecognition().flushEnd();
              await microtasksFinished();
              assertEquals(
                  0,
                  metrics.count(
                      'ContextualTasks.VoiceSearch.StateV2',
                      VoiceSearchState.VOICE_SEARCH_ERROR));
              assertEquals(
                  0,
                  metrics.count(
                      'ContextualTasks.VoiceSearch.StateV2',
                      VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED));
            });
          });

          suite('ErrorPermissionAndLayout', () => {
            test(
                'non-canceling network error keeps voice search open',
                async () => {
                  const inner = parts.innerComposebox;
                  await enterVoiceSearchMode();

                  fireRecognitionError('network');
                  await microtasksFinished();
                  await inner.updateComplete;

                  const voiceSearch = getVoiceSearch();
                  const errorContainer =
                      voiceSearch.shadowRoot.querySelector<HTMLElement>(
                          '#error-container');
                  assertTrue(!!errorContainer);
                  assertFalse(errorContainer.hidden);
                  assertTrue(inner.inVoiceSearchMode);
                  assertTrue(
                      parts.wrapper.hasAttribute('in-voice-search-mode_'));
                  assertEquals(
                      1,
                      metrics.count(
                          'ContextualTasks.VoiceSearch.StateV2',
                          VoiceSearchState.VOICE_SEARCH_ERROR));
                  assertEquals(
                      0,
                      metrics.count(
                          'ContextualTasks.VoiceSearch.StateV2',
                          VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED));
                  assertEquals(
                      0,
                      metrics.count(
                          'ContextualTasks.VoiceSearch.StateV2',
                          VoiceSearchState.VOICE_SEARCH_CANCELED));
                });

            test('error details link is exported and clickable', async () => {
              await enterVoiceSearchMode();
              fireRecognitionError('network');
              await microtasksFinished();

              const voiceSearch = getVoiceSearch();
              await voiceSearch.updateComplete;
              const exportparts = voiceSearch.getAttribute('exportparts');
              assertTrue(!!exportparts);
              const exportedTokens =
                  exportparts.split(',').map(token => token.trim());
              ['voice-close-button', 'voice-details-link', 'voice-stop-button',
               'voice-submit-button']
                  .forEach(token => {
                    assertTrue(
                        exportedTokens.includes(token),
                        `exportparts should include ${token}`);
                  });

              const detailsLink =
                  voiceSearch.shadowRoot.querySelector<HTMLAnchorElement>(
                      '#details');
              assertTrue(!!detailsLink);
              assertTrue(detailsLink.part.contains('voice-details-link'));

              // Isolate the wrapper `::part(voice-details-link)` rule: the
              // element's own `#error-container` rule sets pointer-events: auto
              // on the whole container, which would mask a missing exportparts
              // chain.
              const errorContainer =
                  voiceSearch.shadowRoot.querySelector<HTMLElement>(
                      '#error-container');
              assertTrue(!!errorContainer);
              errorContainer.style.pointerEvents = 'none';
              assertEquals(
                  'auto', window.getComputedStyle(detailsLink).pointerEvents);

              const cancelEvent = eventToPromise<CustomEvent<boolean>>(
                  'voice-search-cancel', voiceSearch);
              detailsLink.click();
              const e = await cancelEvent;
              assertFalse(e.detail, 'Details click is not a user cancel');
              assertEquals(
                  1, mockComposeboxPageHandler.getCallCount('navigateUrl'));
              const navigatedUrl =
                  await mockComposeboxPageHandler.whenCalled('navigateUrl');
              assertTrue(navigatedUrl.includes('support.google.com'));
            });

            test('no-match error cancels voice search', async () => {
              const inner = parts.innerComposebox;
              await enterVoiceSearchMode();

              const recognition = getRecognition();
              assertTrue(!!recognition.onnomatch, 'onnomatch should be wired');
              recognition.onnomatch();
              await microtasksFinished();
              await inner.updateComplete;

              assertFalse(inner.inVoiceSearchMode);
              assertFalse(parts.wrapper.hasAttribute('in-voice-search-mode_'));
              assertEquals(
                  1,
                  metrics.count(
                      'ContextualTasks.VoiceSearch.StateV2',
                      VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED));
              assertEquals(
                  0,
                  metrics.count(
                      'ContextualTasks.VoiceSearch.StateV2',
                      VoiceSearchState.VOICE_SEARCH_CANCELED));
            });

            test('permission prompt pauses listening', async () => {
              loadTimeData.overrideValues(
                  {voiceWaiting: 'Waiting for permission'});
              const inner = parts.innerComposebox;
              await enterVoiceSearchMode();
              const voiceSearch = getVoiceSearch();

              const permissionEvent = eventToPromise<CustomEvent<{
                isOpened: boolean,
              }>>('voice-permission-changed', voiceSearch);
              searchboxCallbackRouterRemote.onPermissionPromptChanged(
                  true, {width: 100, height: 100});
              await searchboxCallbackRouterRemote.$.flushForTesting();
              await microtasksFinished();
              await inner.updateComplete;
              await voiceSearch.updateComplete;

              const permissionState = await permissionEvent;
              assertTrue(permissionState.detail.isOpened);
              assertTrue(voiceSearch.isPermissionPromptOpen);
              assertFalse(inner.isListening);
              assertTrue(
                  getAnimatedGlow().classList.contains(
                      'permission-prompt-showing'));
              assertTrue(
                  voiceSearch.classList.contains('permission-prompt-showing'));
              const waitingInput =
                  voiceSearch.shadowRoot.querySelector<HTMLElement>('#input');
              assertTrue(!!waitingInput);
              assertTrue(
                  waitingInput.textContent.includes('Waiting for permission'));

              searchboxCallbackRouterRemote.onPermissionPromptChanged(
                  false, {width: 0, height: 0});
              await searchboxCallbackRouterRemote.$.flushForTesting();
              await microtasksFinished();
              await inner.updateComplete;
              await voiceSearch.updateComplete;

              assertFalse(voiceSearch.isPermissionPromptOpen);
              assertTrue(inner.isListening);
              assertFalse(
                  getAnimatedGlow().classList.contains(
                      'permission-prompt-showing'));
              assertFalse(
                  voiceSearch.classList.contains('permission-prompt-showing'));
            });

            test(
                'voice search and its container are absolute when not ' +
                    'waiting and not in error',
                async () => {
                  await enterVoiceSearchMode();
                  const voiceSearch = getVoiceSearch();
                  const container =
                      voiceSearch.shadowRoot.querySelector('#container');
                  assertTrue(!!container);

                  assertEquals(
                      'absolute',
                      window.getComputedStyle(voiceSearch).position);
                  assertEquals(
                      coherenceEnabled ? 'absolute' : 'relative',
                      window.getComputedStyle(container).position);

                  // Waiting (permission prompt open):
                  searchboxCallbackRouterRemote.onPermissionPromptChanged(
                      true, {width: 100, height: 100});
                  await searchboxCallbackRouterRemote.$.flushForTesting();
                  await microtasksFinished();
                  await voiceSearch.updateComplete;
                  assertNotEquals(
                      'absolute', window.getComputedStyle(container).position);

                  searchboxCallbackRouterRemote.onPermissionPromptChanged(
                      false, {width: 0, height: 0});
                  await searchboxCallbackRouterRemote.$.flushForTesting();
                  await microtasksFinished();
                  await voiceSearch.updateComplete;

                  // In error:
                  fireRecognitionError('network');
                  await microtasksFinished();
                  await voiceSearch.updateComplete;
                  assertNotEquals(
                      'absolute', window.getComputedStyle(container).position);
                });
          });

          if (!coherenceEnabled) {
            suite('NonCoherenceTranscriptAndCancel', () => {
              test(
                  'live transcript input renders without coherence',
                  async () => {
                    const inner = parts.innerComposebox;
                    assertFalse(parts.wrapper.hasAttribute(
                        'voice-search-coherence-enabled_'));
                    assertFalse(inner.voiceSearchCoherenceEnabled);

                    await enterVoiceSearchMode();
                    const voiceSearch = getVoiceSearch();
                    assertTrue(voiceSearch.liveTranscriptEnabled);
                    assertFalse(voiceSearch.submitStopButtonsEnabled);
                    assertFalse(!!voiceSearch.shadowRoot.querySelector(
                        '#bottomActions'));

                    const closeButton =
                        voiceSearch.shadowRoot.querySelector<HTMLElement>(
                            '#closeButton');
                    assertTrue(!!closeButton);
                    assertTrue(closeButton.part.contains('voice-close-button'));

                    const transcriptText =
                        voiceSearch.shadowRoot.querySelector<HTMLElement>(
                            '#transcript-text');
                    assertTrue(!!transcriptText);
                    assertEquals(
                        loadTimeData.getString('voiceListening'),
                        transcriptText.textContent.trim());

                    fireRecognitionResult(['hello', 'world']);
                    await microtasksFinished();
                    await voiceSearch.updateComplete;
                    assertEquals(
                        'helloworld', transcriptText.textContent.trim());
                  });

              test('user cancel preserves composebox input', async () => {
                const inner = parts.innerComposebox;
                inner.input = 'draft text';
                await inner.updateComplete;

                await enterVoiceSearchMode();
                fireRecognitionResult(['hello']);
                await microtasksFinished();
                await inner.updateComplete;

                assertEquals('draft text', inner.input);
                assertEquals('hello', inner.transcript);
                assertTrue(inner.receivedSpeech);

                const voiceSearch = getVoiceSearch();
                const closeButton =
                    voiceSearch.shadowRoot.querySelector<HTMLElement>(
                        '#closeButton');
                assertTrue(!!closeButton);
                closeButton.click();
                await microtasksFinished();
                await inner.updateComplete;
                getRecognition().flushEnd();
                await microtasksFinished();

                assertEquals('draft text', inner.input);
                assertFalse(inner.inVoiceSearchMode);
                assertEquals('', inner.transcript);
                assertFalse(inner.receivedSpeech);
                assertEquals(
                    1,
                    metrics.count(
                        'ContextualTasks.VoiceSearch.StateV2',
                        VoiceSearchState.VOICE_SEARCH_CANCELED));
                assertEquals(
                    0,
                    metrics.count(
                        'ContextualTasks.VoiceSearch.StateV2',
                        VoiceSearchState.VOICE_SEARCH_ERROR));
                assertEquals(
                    0,
                    metrics.count(
                        'ContextualTasks.VoiceSearch.StateV2',
                        VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED));
              });
            });
          }

          if (coherenceEnabled) {
            async function submitVoiceSearchViaSubmitButton(
                transcripts: string[]) {
              const voiceSearch = getVoiceSearch();
              fireRecognitionResult(transcripts);
              await microtasksFinished();
              await voiceSearch.updateComplete;

              const submitButton =
                  voiceSearch.shadowRoot.querySelector('cr-composebox-submit');
              assertTrue(!!submitButton);
              assertFalse(submitButton.disabled);
              await submitButton.updateComplete;
              const submitContainer =
                  submitButton.shadowRoot.querySelector<HTMLElement>(
                      '#submitContainer');
              assertTrue(!!submitContainer);
              submitContainer.click();
              await microtasksFinished();
              await parts.innerComposebox.updateComplete;
              await mockSearchboxPageHandler.whenCalled('submitQuery');
            }

            async function addImageFile() {
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
              await parts.innerComposebox.updateComplete;
            }

            async function removeImageFromVoiceCarousel(
                voiceCarousel: Element) {
              const shadowRoot = voiceCarousel.shadowRoot;
              assertTrue(!!shadowRoot);
              const fileThumbnail =
                  shadowRoot.querySelector('cr-composebox-file-thumbnail');
              assertTrue(!!fileThumbnail);
              const removeImgButton =
                  fileThumbnail.shadowRoot.querySelector<HTMLElement>(
                      '#removeImgButton');
              assertTrue(!!removeImgButton);
              removeImgButton.click();
              await microtasksFinished();
              await parts.innerComposebox.updateComplete;
            }

            suite('CoherenceControlsFilesAndLifecycle', () => {
              test(
                  'coherence overlay shows stop and submit controls',
                  async () => {
                    const inner = parts.innerComposebox;
                    assertTrue(parts.wrapper.hasAttribute(
                        'voice-search-coherence-enabled_'));
                    assertTrue(inner.voiceSearchCoherenceEnabled);

                    await enterVoiceSearchMode();
                    const voiceSearch = getVoiceSearch();
                    assertFalse(voiceSearch.liveTranscriptEnabled);
                    assertTrue(voiceSearch.submitStopButtonsEnabled);
                    assertFalse(
                        !!voiceSearch.shadowRoot.querySelector('#input'));
                    assertFalse(
                        !!voiceSearch.shadowRoot.querySelector('#closeButton'));

                    assertEquals(
                        SubmitButtonIconType.UPWARD,
                        inner.submitButtonIconType);
                    assertEquals(
                        inner.submitButtonIconType,
                        voiceSearch.submitButtonIconType);

                    const stopButton =
                        voiceSearch.shadowRoot.querySelector<HTMLElement>(
                            '#stopButton');
                    assertTrue(!!stopButton);
                    assertTrue(stopButton.part.contains('voice-stop-button'));
                    assertFalse(stopButton.hasAttribute('disabled'));

                    const submitButton = voiceSearch.shadowRoot.querySelector(
                        'cr-composebox-submit');
                    assertTrue(!!submitButton);
                    assertTrue(
                        submitButton.part.contains('voice-submit-button'));
                    assertEquals(
                        SubmitButtonIconType.UPWARD, submitButton.iconType);
                    assertTrue(submitButton.disabled);
                    assertEquals(
                        '0',
                        window.getComputedStyle(submitButton)
                            .getPropertyValue(
                                '--cr-composebox-submit-icon-offset')
                            .trim());

                    fireRecognitionResult(['hello']);
                    await microtasksFinished();
                    await voiceSearch.updateComplete;
                    assertFalse(submitButton.disabled);
                  });

              test(
                  'recording stopped populates input without submitting',
                  async () => {
                    const inner = parts.innerComposebox;
                    await whenMetricSourceFetched();
                    await enterVoiceSearchMode();

                    fireRecognitionResult(['hello', 'world']);
                    await microtasksFinished();
                    const voiceSearch = getVoiceSearch();
                    await voiceSearch.updateComplete;

                    const stopButton =
                        voiceSearch.shadowRoot.querySelector<HTMLElement>(
                            '#stopButton');
                    assertTrue(!!stopButton);
                    assertFalse(stopButton.hasAttribute('disabled'));
                    stopButton.click();
                    await microtasksFinished();
                    await inner.updateComplete;
                    getRecognition().flushEnd();
                    await microtasksFinished();

                    assertEquals('helloworld', inner.input);
                    assertFalse(inner.inVoiceSearchMode);
                    assertEquals(
                        1,
                        mockSearchboxPageHandler.getCallCount(
                            'queryAutocomplete'));
                    const queryArgs = mockSearchboxPageHandler.getArgs(
                        'queryAutocomplete')[0];
                    assertEquals('helloworld', queryArgs[2]);
                    assertEquals(
                        0,
                        mockSearchboxPageHandler.getCallCount('submitQuery'));
                    assertEquals(
                        1,
                        metrics.count(
                            'VoiceSearch.Action.CO_BROWSING_COMPOSEBOX',
                            VoiceSearchAction.STOP_BUTTON_CLICKED));
                  });

              test('manual submit sends voice query', async () => {
                const inner = parts.innerComposebox;
                await whenMetricSourceFetched();
                await enterVoiceSearchMode();

                await submitVoiceSearchViaSubmitButton(['test', 'query']);

                const submitArgs =
                    await mockSearchboxPageHandler.whenCalled('submitQuery');
                assertEquals(
                    1, mockSearchboxPageHandler.getCallCount('submitQuery'));
                assertEquals('testquery', submitArgs[0]);
                assertTrue(submitArgs[6], 'Should submit as a voice query');
                assertFalse(inner.inVoiceSearchMode);
                assertEquals(
                    1,
                    metrics.count(
                        'VoiceSearch.Action.CO_BROWSING_COMPOSEBOX',
                        VoiceSearchAction.QUERY_SUBMITTED));
              });

              test(
                  'glow reflects showing-only-carousel-on-top-of-input for ' +
                      'image-only state',
                  async () => {
                    const animatedGlow = getAnimatedGlow();
                    assertFalse(animatedGlow.showingOnlyCarouselOnTopOfInput);

                    await addImageFile();

                    assertTrue(animatedGlow.showingOnlyCarouselOnTopOfInput);
                    assertTrue(animatedGlow.hasAttribute(
                        'showing-only-carousel-on-top-of-input'));
                  });

              test(
                  'toolchip and image added, then removed in voice search',
                  async () => {
                    const inner = parts.innerComposebox;
                    // Add tool chip:
                    inner.inToolMode = true;
                    // Add image:
                    await addImageFile();

                    // Enter voice search mode:
                    await enterVoiceSearchMode();

                    // Ensure carousel and toolchip are visible in voice search:
                    const animatedGlow = getAnimatedGlow();
                    const voiceCarouselContainer =
                        animatedGlow.querySelector('#voiceCarouselContainer');
                    assertTrue(!!voiceCarouselContainer);
                    const voiceCarousel = voiceCarouselContainer.querySelector(
                        '#voiceSearchCarousel');
                    assertTrue(!!voiceCarousel);
                    const voiceToolChip =
                        animatedGlow.querySelector('#voiceToolChipsContainer');
                    assertTrue(!!voiceToolChip);

                    // Verify slot assignment into the glow shadow DOM:
                    assertEquals(
                        'carousel',
                        voiceCarouselContainer.getAttribute('slot'));
                    assertEquals(
                        'tool-chip', voiceToolChip.getAttribute('slot'));
                    const carouselSlot =
                        animatedGlow.shadowRoot.querySelector<HTMLSlotElement>(
                            'slot[name="carousel"]');
                    assertTrue(!!carouselSlot);
                    assertTrue(carouselSlot.assignedElements().includes(
                        voiceCarouselContainer));
                    const toolChipSlot =
                        animatedGlow.shadowRoot.querySelector<HTMLSlotElement>(
                            'slot[name="tool-chip"]');
                    assertTrue(!!toolChipSlot);
                    assertTrue(toolChipSlot.assignedElements().includes(
                        voiceToolChip));

                    // Verify CSS order
                    assertTrue(voiceCarousel.classList.contains('top'));
                    assertEquals(
                        '0',
                        window.getComputedStyle(voiceCarouselContainer).order);
                    assertEquals(
                        '3', window.getComputedStyle(voiceToolChip).order);
                    const recordingWave =
                        animatedGlow.shadowRoot.querySelector('#recordingWave');
                    assertTrue(!!recordingWave);
                    assertEquals(
                        '1', window.getComputedStyle(recordingWave).order);

                    // Remove image:
                    await removeImageFromVoiceCarousel(voiceCarousel);
                    assertEquals(0, inner.attachedContext.size);

                    // Remove toolchip:
                    inner.inToolMode = false;
                    await inner.updateComplete;
                    assertFalse(!!animatedGlow.querySelector(
                        '#voiceToolChipsContainer'));
                  });

              test(
                  'remove image but submit toolchip in voice search mode',
                  async () => {
                    const inner = parts.innerComposebox;
                    // Add tool chip and image:
                    inner.inToolMode = true;
                    await addImageFile();

                    await enterVoiceSearchMode();

                    const animatedGlow = getAnimatedGlow();
                    const voiceCarouselContainer =
                        animatedGlow.querySelector('#voiceCarouselContainer');
                    assertTrue(!!voiceCarouselContainer);
                    const voiceCarousel = voiceCarouselContainer.querySelector(
                        '#voiceSearchCarousel');
                    assertTrue(!!voiceCarousel);

                    // Remove image from voice carousel:
                    await removeImageFromVoiceCarousel(voiceCarousel);
                    assertEquals(0, inner.attachedContext.size);

                    // Submit:
                    await submitVoiceSearchViaSubmitButton(['test', 'query']);

                    assertTrue(inner.inToolMode);
                    assertEquals(0, inner.attachedContext.size);
                  });

              test(
                  'remove toolchip but submit image in voice search mode',
                  async () => {
                    const inner = parts.innerComposebox;
                    // Add tool chip and image:
                    inner.inToolMode = true;
                    await addImageFile();

                    await enterVoiceSearchMode();

                    const animatedGlow = getAnimatedGlow();
                    const voiceToolChip =
                        animatedGlow.querySelector('#voiceToolChipsContainer');
                    assertTrue(!!voiceToolChip);

                    // Remove toolchip from voice tool chips container:
                    const toolChip =
                        voiceToolChip.querySelector('cr-composebox-tool-chip');
                    assertTrue(!!toolChip);
                    const toolEnabledButton =
                        toolChip.shadowRoot.querySelector<HTMLElement>(
                            '#toolEnabledButton');
                    assertTrue(!!toolEnabledButton);
                    toolEnabledButton.click();
                    // Prevent the image file from being cleared on component
                    // updates (follows `inputState`):
                    searchboxCallbackRouterRemote.onInputStateChanged(
                        new MockInputState({
                          activeTool: ToolMode.kUnspecified,
                          allowedInputTypes: [InputType.kLensImage],
                        }));
                    await microtasksFinished();
                    await inner.updateComplete;
                    assertFalse(inner.inToolMode);
                    assertEquals(1, inner.attachedContext.size);

                    // Submit:
                    await submitVoiceSearchViaSubmitButton(['test', 'query']);

                    assertFalse(inner.inToolMode);
                    // Submitting resets file count to 0:
                    assertEquals(0, inner.attachedContext.size);
                  });

              test(
                  'removing chips in voice carousel removes them from main ' +
                      'carousel after stopping recording',
                  async () => {
                    const inner = parts.innerComposebox;
                    // Add tool chip and image:
                    inner.inToolMode = true;
                    await addImageFile();

                    // Enter voice search mode by clicking voice search button:
                    await enterVoiceSearchMode();

                    const animatedGlow = getAnimatedGlow();
                    const voiceCarouselContainer =
                        animatedGlow.querySelector('#voiceCarouselContainer');
                    assertTrue(!!voiceCarouselContainer);
                    const voiceCarousel = voiceCarouselContainer.querySelector(
                        '#voiceSearchCarousel');
                    assertTrue(!!voiceCarousel);
                    const voiceToolChip =
                        animatedGlow.querySelector('#voiceToolChipsContainer');
                    assertTrue(!!voiceToolChip);

                    // Remove image from voice carousel:
                    await removeImageFromVoiceCarousel(voiceCarousel);
                    assertEquals(0, inner.attachedContext.size);

                    // Remove tool chip from voice tool chips container:
                    const toolChip =
                        voiceToolChip.querySelector('cr-composebox-tool-chip');
                    assertTrue(!!toolChip);
                    const toolEnabledButton =
                        toolChip.shadowRoot.querySelector<HTMLElement>(
                            '#toolEnabledButton');
                    assertTrue(!!toolEnabledButton);
                    toolEnabledButton.click();
                    searchboxCallbackRouterRemote.onInputStateChanged(
                        new MockInputState({
                          activeTool: ToolMode.kUnspecified,
                          allowedInputTypes: [InputType.kLensImage],
                        }));
                    await microtasksFinished();
                    await inner.updateComplete;
                    assertFalse(inner.inToolMode);

                    // Stop recording:
                    const voiceSearch = getVoiceSearch();
                    const stopButton =
                        voiceSearch.shadowRoot.querySelector<HTMLElement>(
                            '#stopButton');
                    assertTrue(!!stopButton);
                    stopButton.click();
                    await microtasksFinished();
                    await inner.updateComplete;

                    assertFalse(inner.inToolMode);
                    assertEquals(0, inner.attachedContext.size);
                  });
            });
          }
        });
  });
});
