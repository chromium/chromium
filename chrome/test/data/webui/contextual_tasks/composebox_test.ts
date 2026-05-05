// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxToolChipElement} from 'chrome://resources/cr_components/composebox/composebox_tool_chip.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {fixtureUrl, getSubmitButton, setupAutocompleteResults, simulateUserInput} from './test_utils.js';

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
      }] :
                                               [],
    });
    await microtasksFinished();
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
      contextualMenuUsePecApi: false,
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
    });

    testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
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

  test('SelectingMatchPopulatesComposebox', async () => {
    mockTimer.install();
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const inputElement = composebox.getInputElement().$.input;


    const testQuery = 'test';
    simulateUserInput(inputElement, testQuery);

    const matches = [
      createAutocompleteMatch({fillIntoEdit: 'match 1'}),
      createAutocompleteMatch({fillIntoEdit: 'match 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting(
            {input: testQuery, matches: matches}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    mockTimer.tick(0);

    const matchesEl = composebox.getDropdownElement();
    assertTrue(matchesEl.result !== null, 'Matches should be populated');
    assertEquals(2, matchesEl.result.matches.length, 'Should have 2 matches');


    inputElement.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));

    // Wait for Lit updates to propagate
    mockTimer.tick(100);
    await composebox.getDropdownElement().updateComplete;
    await composebox.updateComplete;
    assertEquals(
        0, composebox.getDropdownElement().selectedMatchIndex,
        'Index should be 0');
    assertEquals(
        'match 1', inputElement.value, 'Input value should be match 1');
    assertEquals(0, composebox.selectedMatchIndex, 'Parent index should be 0');
  });

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

  test('ToolChipVisibilityBasedOnInputState', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;

    const getChip = () => {
      const toolChip = $$(innerComposebox, 'cr-composebox-tool-chip');
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

  test('EnterKeyAfterSubmitDoesNotAddNewLine', async () => {
    mockTimer.install();
    const TEST_QUERY = 'test query';

    const inputElement = composebox.getInputElement().$.input;
    assertTrue(
        isVisible(inputElement), 'Composebox input element should be visible');

    // 1. Setup: Input text.
    simulateUserInput(inputElement, TEST_QUERY);

    // Advance timer to trigger the debounced autocomplete query.
    mockTimer.tick(300);

    // Wait for the component to actually request autocomplete
    // before we mock the response, otherwise the response is ignored.
    await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

    // 2. Mock Autocomplete Results.
    await setupAutocompleteResults(searchboxCallbackRouterRemote, TEST_QUERY,
        mockTimer);

    // Wait for the matches to be populated in the dropdown.
    while (!composebox.getDropdownElement().result) {
      mockTimer.tick(10);
      await Promise.resolve();
    }

    const submitButton = getSubmitButton(composebox);
    assertTrue(submitButton !== null);
    assertFalse(submitButton.disabled, 'Submit should be enabled');

    // 3. Action: Simulate Enter press to submit
    mockSearchboxPageHandler.reset();
    pressEnter(inputElement);

    await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');
    assertEquals(
        1, mockSearchboxPageHandler.getCallCount('openAutocompleteMatch'));

    // Tick timer to allow Lit's update lifecycle to process the submit.
    mockTimer.tick(0);

    // 4. Wait for the UI to clear the input after submission.
    await composebox.updateComplete;
    assertEquals(
        '', inputElement.value, 'Input should be cleared after submit');
    assertEquals(
        null, composebox.getDropdownElement().result,
        'Matches should be cleared after submit');

    // 5. Action: Press Enter again on empty input.
    mockSearchboxPageHandler.reset();
    pressEnter(inputElement);

    // Tick timer again for Lit updates.
    mockTimer.tick(0);
    await composebox.updateComplete;

    // 6. Assert: No newline added, submit not called again.
    assertFalse(inputElement.value.includes('\n'));
    assertEquals(0, mockSearchboxPageHandler.getCallCount('submitQuery'));
    assertEquals(
        0, mockSearchboxPageHandler.getCallCount('openAutocompleteMatch'));
  });

  test('EnterKeyOnEmptyInputDoesNotAddNewLineOrSubmit', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;
    const inputElement = innerComposebox.getInputElement().$.input;
    const keydownDiv =
        innerComposebox.shadowRoot.querySelector<HTMLElement>('#composebox');
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

  test('composebox is hidden until isZeroState is not undefined', async () => {
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

    const app = document.createElement('contextual-tasks-app');
    document.body.appendChild(app);

    await app.updateComplete;
    await microtasksFinished();

    // Since connectedCallback immediately resolves the isZeroState promise
    // and sets it to false, we force it back to undefined here to test the
    // initial rendering state.
    // Use undefined to test fallback, matching API optionality
    app.setIsZeroStateForTesting(undefined);
    app.requestUpdate();
    await app.updateComplete;
    await microtasksFinished();

    const composeboxWrapper = app.$.composebox;
    assertTrue(
        composeboxWrapper.hasAttribute('hidden'),
        'Composebox should be hidden when isZeroState is undefined');

    // Mock 'isZeroState_' updating value from parent to true.
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await app.updateComplete;

    assertFalse(
        composeboxWrapper.hasAttribute('hidden'),
        'Composebox should be visible when isZeroState is true');

    // Mock 'isZeroState_' updating value from parent to false.
    testProxy.callbackRouterRemote.onZeroStateChange(false);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await app.updateComplete;

    assertFalse(
        composeboxWrapper.hasAttribute('hidden'),
        'Composebox should be visible when isZeroState is false');
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

  test('lens overlay showing updates placeholder', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;
    const inputElement = innerComposebox.getInputElement().$.input;

    // Initially false, placeholder override should be empty.
    assertFalse(contextualComposebox.isOverlayOpenForAimVisualSearch);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;
    assertEquals('', innerComposebox.inputPlaceholderOverride);

    const initialPlaceholder = inputElement.placeholder;

    // Set to true.
    contextualComposebox.isOverlayOpenForAimVisualSearch = true;
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertTrue(contextualComposebox.isOverlayOpenForAimVisualSearch);
    assertEquals('Test Lens Hint', innerComposebox.inputPlaceholderOverride);
    assertEquals('Test Lens Hint', inputElement.placeholder);

    // Set back to false.
    contextualComposebox.isOverlayOpenForAimVisualSearch = false;
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertFalse(contextualComposebox.isOverlayOpenForAimVisualSearch);
    assertEquals('', innerComposebox.inputPlaceholderOverride);
    assertEquals(initialPlaceholder, inputElement.placeholder);
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

    await contextualComposebox.updateComplete;

    // Simulate Tab focus (match-focusin).
    dropdown.dispatchEvent(new CustomEvent('match-focusin', {
      detail: {index: 0},
      bubbles: true,
      composed: true,
    }));

    await innerComposebox.updateComplete;

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

  test('ClearInputAndFocusClearsMatchesOnSubmit', () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    let clearAutocompleteMatchesCallCount = 0;
    let queryAutocompleteCallCount = 0;

    innerComposebox.clearAutocompleteMatches = () => {
      clearAutocompleteMatchesCallCount++;
    };

    innerComposebox.queryAutocomplete = () => {
      queryAutocompleteCallCount++;
    };

    contextualComposebox.isZeroState = true;
    contextualComposebox.clearInputAndFocus(true);
    assertEquals(
        1, clearAutocompleteMatchesCallCount,
        'querySubmitted = true should clear matches');
    assertEquals(
        0, queryAutocompleteCallCount,
        'querySubmitted = true should not query');
  });

  test('ClearInputAndFocusClearsMatchesWhenNotZeroState', () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    let clearAutocompleteMatchesCallCount = 0;
    let queryAutocompleteCallCount = 0;

    innerComposebox.clearAutocompleteMatches = () => {
      clearAutocompleteMatchesCallCount++;
    };

    innerComposebox.queryAutocomplete = () => {
      queryAutocompleteCallCount++;
    };

    contextualComposebox.isZeroState = false;
    contextualComposebox.clearInputAndFocus(false);
    assertEquals(
        1, clearAutocompleteMatchesCallCount,
        'isZeroState = false should clear matches');
    assertEquals(
        0, queryAutocompleteCallCount, 'isZeroState = false should not query');
  });

  test('ClearInputAndFocusIgnoresEmptyZeroState', () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    let clearAutocompleteMatchesCallCount = 0;
    let queryAutocompleteCallCount = 0;

    innerComposebox.clearAutocompleteMatches = () => {
      clearAutocompleteMatchesCallCount++;
    };

    innerComposebox.queryAutocomplete = () => {
      queryAutocompleteCallCount++;
    };

    contextualComposebox.isZeroState = true;
    innerComposebox.getInputElement().$.input.value = '';
    contextualComposebox.clearInputAndFocus(false);
    assertEquals(
        0, clearAutocompleteMatchesCallCount,
        'hadContent = false should not clear matches');
    assertEquals(
        0, queryAutocompleteCallCount, 'hadContent = false should not query');
  });

  test('ClearInputAndFocusQueriesZeroStateWithText', () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

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

    contextualComposebox.isZeroState = true;
    innerComposebox.input = 'test';
    contextualComposebox.clearInputAndFocus(false);
    assertEquals(
        0, clearAutocompleteMatchesCallCount,
        'hadContent = true should not clear matches');
    assertEquals(
        1, queryAutocompleteCallCount, 'hadContent = true should query');
    assertTrue(
        queryAutocompleteClearMatchesArg, 'should pass clearMatches = true');
  });

  test('ClearInputAndFocusQueriesZeroStateWithFiles', () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

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

    contextualComposebox.isZeroState = true;
    innerComposebox.input = '';
    innerComposebox.hasFiles = () => true;
    contextualComposebox.clearInputAndFocus(false);
    assertEquals(
        0, clearAutocompleteMatchesCallCount,
        'hadContent = true (files) should not clear matches');
    assertEquals(
        1, queryAutocompleteCallCount,
        'hadContent = true (files) should query');
    assertTrue(
        queryAutocompleteClearMatchesArg, 'should pass clearMatches = true');
  });

  test('CanvasChipRemovabilityBasedOnQuerySubmission', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;

    const getChip = () => {
      const toolChip = $$(innerComposebox, 'cr-composebox-tool-chip');
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

    // Simulate navigation without rc=1.
    const loadStartEventNoRc = new Event('loadstart');
    Object.assign(
        loadStartEventNoRc, {url: 'http://example.com', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadStartForTesting(
        loadStartEventNoRc as chrome.webviewTag.LoadStartEvent);

    const loadCommitEventNoRc = new Event('loadcommit');
    Object.assign(
        loadCommitEventNoRc, {url: 'http://example.com', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadCommitForTesting(
        loadCommitEventNoRc as chrome.webviewTag.LoadCommitEvent);
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

    // Simulate navigation with rc=1.
    const loadStartEventWithRc = new Event('loadstart');
    Object.assign(
        loadStartEventWithRc,
        {url: 'http://example.com?rc=1', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadStartForTesting(
        loadStartEventWithRc as chrome.webviewTag.LoadStartEvent);

    const loadCommitEventWithRc = new Event('loadcommit');
    Object.assign(
        loadCommitEventWithRc, {url: 'http://example.com?rc=1', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadCommitForTesting(
        loadCommitEventWithRc as chrome.webviewTag.LoadCommitEvent);
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
    assertFalse(contextualTasksApp.$.composebox.isCanvasQuerySubmitted);
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
});
