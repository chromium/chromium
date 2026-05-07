// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For composebox tests related to tools, secondary inputs (voice, drag/drop).
import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextUploadStatus, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {WindowOpenDisposition} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {ADD_FILE_CONTEXT_FN, assertStyle, FAKE_TOKEN_STRING, fixtureUrl, getSubmitButton, getSubmitContainer, installMock, setupAutocompleteResults, simulateUserInput, uploadFileAndVerify} from './test_utils.js';

function disableAnimationsRecursively(element: Element) {
  const noAnimation = document.createElement('style');
  noAnimation.textContent = `
    :host, * {
    transition: none !important;
    animation: none !important;
    transition-duration: 0s !important;
    animation-duration: 0s !important;
    }
  `;

  if (element.shadowRoot) {
    element.shadowRoot.appendChild(noAnimation);

    const children = element.shadowRoot.querySelectorAll('*');
    children.forEach(child => disableAnimationsRecursively(child));
  }
}

// Checks if suggestions container is rendered yet.
function checkIfCanFindSuggestionsContainer(
    contextualTasksApp: ContextualTasksAppElement, canFind: boolean) {
  const suggestions = contextualTasksApp.$.composebox.shadowRoot.querySelector(
      '#contextualTasksSuggestionsContainer');

  if (canFind) {
    assertTrue(!!suggestions, 'Suggestions container should be present in DOM');
  } else {
    assertEquals(
        null, suggestions, 'Suggestions container should not be in DOM');
  }
}

suite('ContextualTasksComposeboxZeroStateTest', () => {
  let contextualTasksApp: ContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let windowProxy: TestMock<WindowProxy>;
  let mockTimer: MockTimer;

  async function setActiveTool(tool: ToolMode) {
    searchboxCallbackRouterRemote.onInputStateChanged({
      ...new MockInputState(),
      activeTool: tool,
    });
    await microtasksFinished();
  }

  setup(async () => {
    const win = window as any;

    if (!win.chrome) {
      win.chrome = {};
    }

    if (!win.chrome.histograms) {
      win.chrome.histograms = {
        recordEnumerationValue: () => {},
        recordUserAction: () => {},
        recordBoolean: () => {},
      };
    }

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockTimer = new MockTimer();

    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
      forcedEmbeddedPageHost: '',
    });

    testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockComposeboxPageHandler.setResultFor(
        'getSmartTabSharingActive', Promise.resolve({active: false}));
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler as any, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app');
    await customElements.whenDefined('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);

    await contextualTasksApp.updateComplete;
    await microtasksFinished();

    disableAnimationsRecursively(contextualTasksApp);

    composebox = contextualTasksApp.$.composebox.$.composebox;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);

    windowProxy.setResultFor('matchMedia', {
      matches: false,
      addEventListener: () => {},
      removeEventListener: () => {},
    });

    searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState());
    await microtasksFinished();

    // mockTimer.install() is NOT called here because many tests use real
    // setTimeout via microtasksFinished(). Tests that need it should call it
    // themselves after setup is done.
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  test(
      'suggestions show correctly in side panel zero state based on loading',
      async () => {
        testProxy.handler.setIsShownInTab(false);

        testProxy.callbackRouterRemote.onZeroStateChange(true);
        testProxy.callbackRouterRemote.onSidePanelStateChanged();

        await testProxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(false);

        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(contextualTasksApp.$.composebox, 'bottom', '0px');

        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ false);

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(true);

        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composebox, 'bottom', '30px',
            'bottom should be 30px');

        contextualTasksApp.$.composebox.setIsLoadingForTesting(true);
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();
        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ true);
        const firstMatch: any =
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.shadowRoot.querySelector(
                    '#match0');
        assertTrue(
            !!firstMatch.$.textContainer,
            'First suggestion match should exist');
        assertStyle(
            firstMatch.$.textContainer, 'animation-duration', '2s',
            'When in loading side panel but in zero-state, suggestions\
                should be in loading state');

        contextualTasksApp.$.composebox.setIsLoadingForTesting(false);
        await contextualTasksApp.$.composebox.updateComplete;
        await firstMatch.updateComplete;
        await microtasksFinished();

        assertStyle(
            firstMatch.$.textContainer, 'animation', 'none',
            'When not in loading side panel zero-state,\
                suggestions should be normal');

        testProxy.callbackRouterRemote.onZeroStateChange(false);

        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertTrue(
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.hidden,
            'Dropdown should be hidden when NOT in zero state',
        );
      });

  test(
      'suggestions show correctly in full tab zero state based on loading',
      async () => {
        testProxy.handler.setIsShownInTab(true);

        testProxy.callbackRouterRemote.onZeroStateChange(true);
        testProxy.callbackRouterRemote.onSidePanelStateChanged();

        await testProxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(false);

        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ false);

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(true);

        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ true);
        assertStyle(
            contextualTasksApp.$.flexCenterContainer, 'top', '88px',
            'top should be 88px');

        contextualTasksApp.$.composebox.setIsLoadingForTesting(true);
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();
        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ true);
        const firstMatch: any =
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.shadowRoot.querySelector(
                    '#match0');
        assertTrue(
            !!firstMatch.$.textContainer,
            'First suggestion match should exist');

        assertStyle(
            firstMatch.$.textContainer, 'animation-duration', '2s',
            'When in loading full tab zero-state,\
                suggestions should be in loading state');
        contextualTasksApp.$.composebox.setIsLoadingForTesting(false);
        await contextualTasksApp.$.composebox.updateComplete;
        await firstMatch.updateComplete;
        await microtasksFinished();

        assertStyle(
            firstMatch.$.textContainer, 'animation', 'none',
            'When not in loading full tab but in zero-state,\
                suggestions should be normal');

        testProxy.callbackRouterRemote.onZeroStateChange(false);

        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertTrue(
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.hidden,
            'Dropdown should be hidden when NOT in zero state',
        );
      });

  test('no suggestions in zero state when inNlm is true', async () => {
    testProxy.handler.setIsShownInTab(true);

    testProxy.callbackRouterRemote.onZeroStateChange(true);
    testProxy.callbackRouterRemote.onSidePanelStateChanged();

    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(true);
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    checkIfCanFindSuggestionsContainer(contextualTasksApp, /*canFind=*/ true);

    contextualTasksApp.setInNlmForTesting(true);
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertTrue(
        contextualTasksApp.$.composebox.$.contextualTasksSuggestionsContainer
            .hidden,
        'Dropdown should be hidden when in NLM mode',
    );
  });

  test('zero state animation plays when zero state changes', async () => {
    loadTimeData.overrideValues({
      friendlyZeroStateGaiaName: 'Test Name',
    });

    // Re-create the app to ensure it picks up the new loadTimeData values.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    contextualTasksApp = document.createElement('contextual-tasks-app');
    await customElements.whenDefined('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);
    await contextualTasksApp.updateComplete;
    await microtasksFinished();

    // Set initial state to true so we can transition to false then back to
    // true.
    testProxy.callbackRouterRemote.onZeroStateChange(/*isZeroState=*/ true);
    testProxy.handler.setIsAiPage(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await contextualTasksApp.updateComplete;
    await microtasksFinished();

    // Simulate the initial load.
    let resolveInitNavigation: () => void;
    const initNavigationFinished = new Promise<void>(r => resolveInitNavigation = r);
    contextualTasksApp.setOnLoadStartFinishedCallbackForTesting(
        resolveInitNavigation!);

    const initEvent = new Event('loadstart');
    Object.assign(initEvent, {url: 'http://example.com', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadStartForTesting(
        initEvent as chrome.webviewTag.LoadStartEvent);

    const initCommitEvent = new Event('loadcommit');
    Object.assign(initCommitEvent, {url: 'http://example.com', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadCommitForTesting(
        initCommitEvent as chrome.webviewTag.LoadCommitEvent);

    await initNavigationFinished;

    const composeboxWrapper = contextualTasksApp.$.composebox;

    const headerWrapper = contextualTasksApp.$.composeboxHeaderWrapper;
    // nameShimmer might not exist if friendlyZeroStateGaiaName_ is not set.
    const nameShimmer = contextualTasksApp.$.nameShimmer;

    // Mock animate function.
    let composeboxAnimateCalled = false;
    let headerAnimateCalled = false;
    let nameShimmerAnimateCalled = false;

    let resolveCompose: (value: any) => void;
    let resolveHeader: (value: any) => void;
    let resolveNameShimmer: (value: any) => void;

    const promisesToWaitOn = [
      new Promise(r => resolveCompose = r),
      new Promise(r => resolveHeader = r),
    ];

    if (nameShimmer) {
      promisesToWaitOn.push(new Promise(r => resolveNameShimmer = r));
    }

    const animationsStarted = Promise.all(promisesToWaitOn);

    // Transition out of zero state first.
    testProxy.callbackRouterRemote.onZeroStateChange(false);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    // Mock getAnimations to return dummy animations that can be cancelled and
    // played.
    const createMockAnimation = (callback: () => void) =>
        ({
          cancel: () => {},
          play: () => {
            callback();
            return Promise.resolve();
          },
        }) as unknown as Animation;

    composeboxWrapper.getAnimations = () => [createMockAnimation(() => {
      composeboxAnimateCalled = true;
      resolveCompose(true);
    })];

    headerWrapper.getAnimations = () => [createMockAnimation(() => {
      headerAnimateCalled = true;
      resolveHeader(true);
    })];

    if (nameShimmer) {
      nameShimmer.getAnimations = () => [createMockAnimation(() => {
        nameShimmerAnimateCalled = true;
        resolveNameShimmer(true);
      })];
    }

    // Mock startExpandAnimation since it is called to trigger the glow
    // animation.
    contextualTasksApp.$.composebox.startExpandAnimation =
        () => Promise.resolve();

    testProxy.handler.setIsZeroState(true);
    // Transition back to zero state via mock.
    // Thread frame load start to trigger animations.
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    let resolveNavigation: () => void;
    const navigationFinished = new Promise<void>(r => resolveNavigation = r);
    contextualTasksApp.setOnLoadStartFinishedCallbackForTesting(
        resolveNavigation!);

    const event = new Event('loadstart');
    Object.assign(event, {url: 'http://example.com', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadStartForTesting(
        event as chrome.webviewTag.LoadStartEvent);

    const commitEvent = new Event('loadcommit');
    Object.assign(commitEvent, {url: 'http://example.com', isTopLevel: true});
    contextualTasksApp.onThreadFrameLoadCommitForTesting(
        commitEvent as chrome.webviewTag.LoadCommitEvent);

    await navigationFinished;
    await animationsStarted;

    // Verify animations were played.
    assertTrue(composeboxAnimateCalled, 'Composebox animation should play');
    assertTrue(headerAnimateCalled, 'Header animation should play');
    if (nameShimmer) {
      assertTrue(
          nameShimmerAnimateCalled, 'Name shimmer animation should play');
    }
  });

  test('SuggestionsHiddenWhenDropdownNotShown', async () => {
    loadTimeData.overrideValues({
      enableNativeZeroStateSuggestions: true,
    });

    // Re-create the app to ensure it picks up the new loadTimeData values.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    contextualTasksApp = document.createElement('contextual-tasks-app');
    await customElements.whenDefined('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);
    await contextualTasksApp.updateComplete;
    await microtasksFinished();

    disableAnimationsRecursively(contextualTasksApp);
    composebox = contextualTasksApp.$.composebox.$.composebox;

    testProxy.handler.setIsShownInTab(true);

    testProxy.callbackRouterRemote.onZeroStateChange(true);
    testProxy.callbackRouterRemote.onSidePanelStateChanged();

    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    const contextualComposebox = contextualTasksApp.$.composebox;
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    const suggestionsContainer =
        contextualComposebox.$.contextualTasksSuggestionsContainer;
    assertTrue(!!suggestionsContainer, 'Suggestions container should exist');

    // Initial state: No matches yet, so show-dropdown should be false.
    assertFalse(
        composebox.hasAttribute('show-dropdown'),
        'Dropdown should not be shown initially');
    assertEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be hidden when dropdown is not shown');

    // Add a file.
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, file, composebox, mockSearchboxPageHandler);

    // Provide ZPS matches (empty query).
    await setupAutocompleteResults(
        searchboxCallbackRouterRemote, '', mockTimer);
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    // show-dropdown should be true now because we have ZPS matches and no
    // input.
    assertTrue(
        composebox.hasAttribute('show-dropdown'),
        'Dropdown should be shown with ZPS matches after adding a file');

    // The suggestions container should be visible.
    assertNotEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be visible when dropdown is shown');

    // Simulate typing.
    const inputElement = composebox.getInputElement().$.input;
    simulateUserInput(inputElement, 'test');

    // Provide typed matches.
    await setupAutocompleteResults(
        searchboxCallbackRouterRemote, 'test', mockTimer);
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    // show-dropdown should be false because we have a file and
    // showTypedSuggestWithContext is false.
    assertFalse(
        composebox.hasAttribute('show-dropdown'),
        'Dropdown should hide when typing with' +
            ' a file and showTypedSuggestWithContext is false');

    // The CSS rule should hide the suggestions container.
    assertEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be hidden via CSS when dropdown is hidden');
  });

  test('TooltipImpressionTimerResetsOnHide', async () => {
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
      composeboxShowOnboardingTooltipImpressionDelay: 3000,
    });

    // Re-create the app to ensure it picks up the new loadTimeData values.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    contextualTasksApp = document.createElement('contextual-tasks-app');
    await customElements.whenDefined('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);
    await contextualTasksApp.updateComplete;
    await microtasksFinished();

    mockTimer.install();

    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = contextualTasksApp.$.onboardingTooltip;
    contextualTasksApp.numberOfTimesTooltipShownForTesting = 0;
    contextualTasksApp.userDismissedTooltipForTesting = false;

    const innerComposebox = composeboxElement.$.composebox;
    // Mock existence of chip.
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    // Show tooltip.
    contextualTasksApp.updateTooltipVisibilityForTesting();
    assertTrue(tooltip!.shouldShow);

    // Advance time partially.
    mockTimer.tick(1000);
    assertEquals(0, contextualTasksApp.numberOfTimesTooltipShownForTesting);

    // Hide tooltip (e.g. chip disappears).
    innerComposebox.getHasAutomaticActiveTabChipToken = () => false;
    contextualTasksApp.updateTooltipVisibilityForTesting();
    assertFalse(tooltip!.shouldShow);

    // Advance past original deadline.
    mockTimer.tick(5000);
    // Should NOT have incremented because timer was cleared.
    assertEquals(0, contextualTasksApp.numberOfTimesTooltipShownForTesting);
  });

  test(
      'on focus out does not set animation state as none \
        when submitting or listening',
      async () => {
        const composebox = contextualTasksApp.$.composebox.$.composebox;

        composebox.animationState = GlowAnimationState.SUBMITTING;
        composebox.dispatchEvent(new CustomEvent('composebox-focus-out', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertEquals(composebox.animationState, GlowAnimationState.SUBMITTING);

        composebox.animationState = GlowAnimationState.LISTENING;
        composebox.dispatchEvent(new CustomEvent('composebox-focus-out', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertEquals(composebox.animationState, GlowAnimationState.LISTENING);
      });

  test('on focus out sets animation state as none otherwise', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    composebox.animationState = GlowAnimationState.EXPANDING;
    composebox.dispatchEvent(new CustomEvent('composebox-focus-out', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(composebox.animationState, GlowAnimationState.NONE);
  });

  test(
      'side panel handles AIM queries to show side panel zero state correctly',
      async () => {
        testProxy.handler.setIsShownInTab(false);

        testProxy.callbackRouterRemote.onZeroStateChange(false);
        testProxy.callbackRouterRemote.onSidePanelStateChanged();

        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composeboxHeader, 'font-size', '28px',
            'When in side panel non-zero-state, composebox header font-size');
        assertStyle(
            contextualTasksApp.$.composebox.$.composebox, 'min-width', '200px',
            'When in side panel non-zero-state, composebox min-width');

        testProxy.callbackRouterRemote.onZeroStateChange(true);

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
            'relative');
        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer,
            'margin-bottom', '0px',
            'When in side panel zero-state, composebox wrapper margin');
        assertStyle(
            contextualTasksApp.$.composebox, 'position', 'relative',
            'When in side panel zero-state, composebox position');

        testProxy.callbackRouterRemote.onZeroStateChange(false);

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
            'relative', 'When returning to side panel non-zero-state,\
                  composebox wrapper position');
        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer,
            'margin-bottom', '30px',
            'When returning to side panel non-zero-state,\
                  composebox wrapper margin');
        assertStyle(
            contextualTasksApp.$.composebox, 'position', 'relative',
            'When returning to side panel non-zero-state,\
                  composebox position');
      });

  test('full tab handles AIM queries to show 0 state correctly', async () => {
    testProxy.handler.setIsShownInTab(true);

    testProxy.callbackRouterRemote.onZeroStateChange(false);
    testProxy.callbackRouterRemote.onSidePanelStateChanged();
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertStyle(
        contextualTasksApp.$.composebox.$.composebox, 'min-width', '0px',
        'When in full tab mode non-zero-state, composebox min-width');
    assertStyle(
        contextualTasksApp.$.composeboxHeader, 'font-size', '32px',
        'When in full tab mode non-zero-state, composebox header font-size');

    testProxy.callbackRouterRemote.onZeroStateChange(true);

    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
        'relative',
        'When in full tab mode zero-state, composebox wrapper position');
    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'margin-bottom',
        '0px', 'When in full tab mode zero-state, composebox wrapper margin');
    assertStyle(
        contextualTasksApp.$.composebox, 'position', 'relative',
        'When in full tab mode zero-state, composebox wrapper position');

    testProxy.callbackRouterRemote.onZeroStateChange(false);

    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
        'relative', 'When returning to full tab mode non-zero-state,\
              composebox wrapper position');
    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'margin-bottom',
        '30px', 'When returning to full tab non-zero-state,\
              composebox wrapper margin');
    assertStyle(
        contextualTasksApp.$.composebox, 'position', 'relative',
        'When returning to full tab mode non-zero-state,\
              composebox position');
  });

  test(
      'Upload status is tracked properly when adding file via browser',
      async () => {
        const fileInfo = {
          fileName: 'test-image.png',
          imageDataUrl: 'data:image/png;base64,xxxx',
          isDeletable: true,
        };
        mockSearchboxPageHandler.setResultFor(
            ADD_FILE_CONTEXT_FN, Promise.resolve({token: FAKE_TOKEN_STRING}));
        composebox.addFileContextFromBrowser(FAKE_TOKEN_STRING, fileInfo);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        const remaining = composebox.pendingUploads;

        assertEquals(1, remaining.size, 'Pending uploads should increase');
        assertTrue(
            remaining.has(FAKE_TOKEN_STRING),
            'The set should contain our specific UUID');

        assertFalse(
            composebox.fileUploadsComplete,
            'fileUploadsComplete should be false');

        const submitButton: HTMLButtonElement|null =
            getSubmitButton(composebox);

        assertTrue(!!submitButton, 'Submit button should exist');
        assertTrue(submitButton?.disabled, 'Submit button should be disabled');
        // Simulate tab upload success.
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kUploadSuccessful,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        const submitContainer: HTMLElement|null =
            getSubmitContainer(composebox);
        assertTrue(!!submitContainer, 'Submit container button should exist');
        assertFalse(
            submitButton?.disabled, 'Submit button should not be disabled');

        assertStyle(
            submitButton, 'pointer-events', 'auto',
            'Submit button should not be disabled');
        assertStyle(
            submitContainer, 'cursor', 'pointer',
            'Submit button cursor should be pointer');
        assertTrue(!!submitContainer, 'Submit container button should exist');

        submitContainer?.click();

        // Flush the macrotask queue / event loop
        await new Promise(resolve => setTimeout(resolve, 0));
        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(0, composebox.files.size);

        // Should be no longer `EXPANDING` after successful upload and submit
        // click.
        assertNotEquals(
            composebox.animationState, GlowAnimationState.EXPANDING);
      });



  interface ToolModeInfo {
    toolMode: ToolMode;
    text: string;
  }

  [{
    toolMode: ToolMode.kDeepSearch,
    text: 'Deep Search',
  },
   {
     toolMode: ToolMode.kImageGen,
     text: 'Create Images',
   },
   {
     toolMode: ToolMode.kCanvas,
     text: 'Canvas',
   }].forEach((toolModeInfo: ToolModeInfo) => {
    test(toolModeInfo.text + ': thread change resets input', async () => {
      await setActiveTool(toolModeInfo.toolMode);

      await composebox.updateComplete;
      await microtasksFinished();

      let toolChip =
          composebox.shadowRoot.querySelector('cr-composebox-tool-chip');

      assertTrue(!!toolChip, toolModeInfo.text + ' chip should be present');

      testProxy.callbackRouterRemote.onZeroStateChange(/*isZeroState=*/ true);
      await testProxy.callbackRouterRemote.$.flushForTesting();

      await composebox.updateComplete;
      await microtasksFinished();

      toolChip = composebox.shadowRoot.querySelector('cr-composebox-tool-chip');
      assertFalse(!!composebox.input, 'Input value should be cleared');
      assertTrue(
          composebox.fileUploadsComplete, 'File uploads should be complete');
      assertFalse(!!composebox.result, 'Autocomplete result should be cleared');
    });
  });

  test('Multiple files updates zero state placeholder', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    const token1 = {high: 0n, low: 1n} as any;
    const token2 = {high: 0n, low: 2n} as any;
    innerComposebox.addFileContextForTesting(
        {type: 'image/png', uuid: token1} as ComposeboxFile);
    innerComposebox.addFileContextForTesting(
        {type: 'application/pdf', uuid: token2} as ComposeboxFile);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertEquals(
        'Ask about these',
        innerComposebox.getInputElement().$.input.placeholder);
  });

  test('Single tab file updates zero state placeholder', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    const token = {high: 0n, low: 1n} as any;
    innerComposebox.addFileContextForTesting(
        {type: 'tab', uuid: token} as ComposeboxFile);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertEquals(
        'Ask about this tab',
        innerComposebox.getInputElement().$.input.placeholder);
  });

  test('Single image file updates zero state placeholder', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    const token = {high: 0n, low: 1n} as any;
    innerComposebox.addFileContextForTesting(
        {type: 'image/png', uuid: token} as ComposeboxFile);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertEquals(
        'Ask about this image',
        innerComposebox.getInputElement().$.input.placeholder);
  });

  test('Single pdf file updates zero state placeholder', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    const token = {high: 0n, low: 1n} as any;
    innerComposebox.addFileContextForTesting(
        {type: 'application/pdf', uuid: token} as ComposeboxFile);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertEquals(
        'Ask about this doc',
        innerComposebox.getInputElement().$.input.placeholder);
  });

  test('Single unknown file updates zero state placeholder', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    const token = {high: 0n, low: 1n} as any;
    innerComposebox.addFileContextForTesting(
        {type: 'unknown/type', uuid: token} as ComposeboxFile);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertFalse(innerComposebox.getInputElement().$.input.placeholder.includes(
        'Ask about'));
  });

  test('Overlay hint text overridden by file hint', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;

    // Set overlay hint text to true.
    contextualComposebox.isOverlayOpenForAimVisualSearch = true;

    // Add an image file.
    const token = {high: 0n, low: 1n} as any;
    innerComposebox.addFileContextForTesting(
        {type: 'image/png', uuid: token} as ComposeboxFile);

    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    // File hint should take precedence over overlay hint.
    assertEquals(
        'Ask about this image',
        innerComposebox.getInputElement().$.input.placeholder);
  });

  test('Arrow in zero state is ignored in full tab', async () => {
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    testProxy.handler.setIsShownInTab(true);

    testProxy.callbackRouterRemote.onSidePanelStateChanged();
    await microtasksFinished();

    const event = new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      cancelable: true,
      bubbles: true,
      composed: true,
    });

    composebox.dispatchEvent(event);
    await microtasksFinished();

    // DropdownNeeded by default is supposed to be false, so arrow
    // keys should be ignored.
    assertEquals(
        composebox.input, '',
        'Input should not change since arrow down does not select suggestion');
    assertEquals(
        composebox.selectedMatchIndex, -1,
        'No suggestion should be selected on arrow down in zero state full tab');
    const event2 = new KeyboardEvent('keydown', {
      key: 'ArrowUp',
      cancelable: true,
      bubbles: true,
      composed: true,
    });

    composebox.dispatchEvent(event2);
    await microtasksFinished();

    assertEquals(
        composebox.input, '',
        'Input should not change since arrow up does not select suggestion');
    assertEquals(
        composebox.selectedMatchIndex, -1,
        'No suggestion should be selected on arrow up in zero state full tab');
  });

  test('Arrow in zero state is ignored in side panel', async () => {
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    testProxy.handler.setIsShownInTab(false);  // side panel

    testProxy.callbackRouterRemote.onSidePanelStateChanged();
    await microtasksFinished();

    const event = new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      cancelable: true,
      bubbles: true,
      composed: true,
    });

    composebox.dispatchEvent(event);
    await microtasksFinished();

    // DropdownNeeded by default is supposed to be false, so arrow
    // keys should be ignored.
    assertEquals(
        composebox.input, '',
        'Input should not change since arrow down does not select suggestion');
    assertEquals(
        composebox.selectedMatchIndex, -1,
        'No suggestion should be selected on arrow down in zero state full tab');
    const event2 = new KeyboardEvent('keydown', {
      key: 'ArrowUp',
      cancelable: true,
      bubbles: true,
      composed: true,
    });

    composebox.dispatchEvent(event2);
    await microtasksFinished();

    assertEquals(
        composebox.input, '',
        'Input should not change since arrow up does not select suggestion');
    assertEquals(
        composebox.selectedMatchIndex, -1,
        'No suggestion should be selected on arrow up in zero state full tab');
  });

  test('clicking activity link calls openUrl', async () => {
    loadTimeData.overrideValues({
      suggestionActivityLink:
          'Learn more about <a href="https://google.com/">activity</a>',
    });

    testProxy.callbackRouterRemote.onZeroStateChange(true);
    testProxy.callbackRouterRemote.onSidePanelStateChanged();

    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    const contextualComposebox = contextualTasksApp.$.composebox;
    // Manual trigger since it depends on results.
    contextualComposebox.$.composebox.dispatchEvent(
        new CustomEvent('show-suggestion-activity-link', {detail: true}));
    await contextualComposebox.updateComplete;

    const activityLink =
        contextualComposebox.shadowRoot.querySelector('localized-link');
    assertTrue(!!activityLink, 'Activity link should be present');

    const anchor = activityLink.shadowRoot.querySelector('a');
    assertTrue(!!anchor, 'Anchor tag should be present');

    anchor.click();
    await microtasksFinished();

    const [url, disposition] = await testProxy.handler.whenCalled('openUrl');
    assertEquals('https://google.com/', url);
    assertEquals(WindowOpenDisposition.NEW_FOREGROUND_TAB, disposition);
  });

});
