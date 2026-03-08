// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {assertStyle, installMock, mockInputState, setupAutocompleteResults, simulateUserInput, uploadFileAndVerify} from './test_utils.js';

const ADD_TAB_CONTEXT_FN = 'addTabContext';
const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';
const FAKE_TOKEN_STRING_2 = '00000000000000001234567890ABCDFF';

function pressEnter(element: HTMLElement) {
  element.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Enter',
    bubbles: true,
    composed: true,
  }));
}

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

suite('ContextualTasksComposeboxSubmitTest', () => {
  let contextualTasksApp: ContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let windowProxy: TestMock<WindowProxy>;
  let mockTimer: MockTimer;

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
    });

    testProxy = new TestContextualTasksBrowserProxy('https://google.com');
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);

    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler as any, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app');
    await customElements.whenDefined('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);

    await microtasksFinished();

    disableAnimationsRecursively(contextualTasksApp);

    composebox = contextualTasksApp.$.composebox.$.composebox;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                            addListener() {},
                                            addEventListener() {},
                                            removeListener() {},
                                            removeEventListener() {},
                                            }));

    searchboxCallbackRouterRemote.onInputStateChanged(mockInputState);
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });


  function getSubmitContainer(): HTMLElement|null {
    return composebox.shadowRoot.querySelector('#submitContainer');
  }

  function getSubmitButton(): HTMLButtonElement|null {
    const submitContainer: HTMLElement|null = getSubmitContainer();

    if (!submitContainer) {
      return null;
    }
    const submitButton: HTMLButtonElement|null =
        submitContainer.querySelector('#submitIcon');
    return submitButton;
  }

  test('submit enabled when tool is Deep Search', async () => {
    const submitContainer = getSubmitContainer();
    assertFalse(
        isVisible(submitContainer), 'Submit container should be hidden');

    // Ensure we start in Zero State (disabled).
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await microtasksFinished();

    // Verify submit button is disabled and clicking it does nothing.
    const submitButton = getSubmitButton();
    assertTrue(submitButton!.disabled, 'Submit button should be disabled');
    submitButton!.click();
    await microtasksFinished();
    assertEquals(
        mockSearchboxPageHandler.getCallCount('submitQuery'), 0,
        'Submit query should not be called when button is disabled');

    // Change tool to Deep Search
    const inputState = Object.assign({}, mockInputState, {
      activeTool: ToolMode.kDeepSearch,
    });
    searchboxCallbackRouterRemote.onInputStateChanged(inputState);
    await searchboxCallbackRouterRemote.$.flushForTesting();

    await microtasksFinished();

    // Verify submit button is disabled and clicking it still does nothing.
    assertTrue(submitButton!.disabled, 'Submit button should be disabled');
    submitButton!.click();
    await microtasksFinished();
    assertEquals(
        mockSearchboxPageHandler.getCallCount('submitQuery'), 0,
        'Submit query should not be called when button is disabled');

    // Set isZeroState to false (simulating follow-up) to allow empty query
    // for Deep Search.
    testProxy.callbackRouterRemote.onZeroStateChange(false);
    await microtasksFinished();
    await composebox.updateComplete;

    // Submit should be enabled now, clicking triggers the action.
    submitButton!.click();
    await microtasksFinished();
    assertEquals(mockSearchboxPageHandler.getCallCount('submitQuery'), 1);
  });

  test('ComposeboxSubmitSendsQueryAndClearsInput', async () => {
    mockTimer.install();
    const TEST_QUERY = 'test query';

    const inputElement = composebox.$.input;
    assertTrue(
        isVisible(inputElement), 'Composebox input element should be visible');

    simulateUserInput(inputElement, TEST_QUERY);

    mockTimer.tick(300);

    await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

    await setupAutocompleteResults(
        searchboxCallbackRouterRemote, TEST_QUERY, mockTimer);

    // Wait for the matches to be populated.
    while (!composebox.getMatchesElement().result) {
      mockTimer.tick(10);
      await Promise.resolve();
    }

    pressEnter(inputElement);

    const [matchIndex, url] =
        await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');

    assertEquals(0, matchIndex);
    assertEquals(`https://google.com/search?q=${TEST_QUERY}`, url);
    mockTimer.tick(0);

    // Cannot use `await microTasksFinished()` here because the transition to
    // zero state triggers `clearAllInputs()`, which modifies the DOM layout.
    // This causes `ResizeObserver` events that schedule additional microtasks,
    // preventing `microTasksFinished()` from settling within the test timeout.
    await composebox.updateComplete;
    await contextualTasksApp.updateComplete;

    assertEquals(
        '', inputElement.value,
        'Input should be cleared, but input = ' + inputElement.value);
    assertEquals(
        null, composebox.getMatchesElement().result,
        'Matches should be cleared');
  });

  test('LensButtonTriggersOverlay', async () => {
    contextualTasksApp.$.composebox.isSidePanel = true;
    await microtasksFinished();

    assertTrue(composebox.lensButtonTriggersOverlay);

    const lensButton = composebox.shadowRoot.querySelector('#lensIcon');
    assertTrue(
        lensButton instanceof HTMLElement,
        'Lens button should be an HTMLElement');
    lensButton.click();

    await mockComposeboxPageHandler.whenCalled('handleLensButtonClick');
    assertEquals(
        1, mockComposeboxPageHandler.getCallCount('handleLensButtonClick'));


    // A second click should still trigger the same handler and the button
    // should still be disabled.
    assertFalse(composebox.lensButtonDisabled);
    lensButton.click();

    await mockComposeboxPageHandler.whenCalled('handleLensButtonClick');
    assertEquals(
        2, mockComposeboxPageHandler.getCallCount('handleLensButtonClick'));
  });

  test(
      'hides composebox and header using z-index when hideInput called',
      async () => {
        const threadFrame = contextualTasksApp.$.threadFrame;
        const flexCenterContainer = contextualTasksApp.$.flexCenterContainer;

        (contextualTasksApp as any).isAiPage_ = false;
        (contextualTasksApp as any).isNavigatingFromAiPage_ = false;

        testProxy.callbackRouterRemote.hideInput();
        await testProxy.callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertFalse(
            composebox.hidden,
            'Composebox should NOT be hidden with z-order flag');

        const threadFrameStyle = getComputedStyle(threadFrame);
        const flexCenterStyle = getComputedStyle(flexCenterContainer);

        assertEquals(
            '1', threadFrameStyle.zIndex, 'Thread frame z-index should be 1');
        assertEquals(
            '0', flexCenterStyle.zIndex,
            'Flex center container z-index should be 0');

        testProxy.callbackRouterRemote.restoreInput();
        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.updateComplete;

        await microtasksFinished();

        const threadFrameStyleRestored = getComputedStyle(threadFrame);
        const flexCenterStyleRestored = getComputedStyle(flexCenterContainer);

        assertFalse(
            threadFrameStyleRestored.zIndex === '1',
            'Thread frame z-index should not be 1 after restore');
        assertFalse(
            flexCenterStyleRestored.zIndex === '0',
            'Flex center container z-index should not be 0 after restore');
      });

  test(
      'hides composebox and header when hideInput called' +
          'and enableBasicModeZOrder is false',
      async () => {
        loadTimeData.overrideValues({enableBasicModeZOrder: false});
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        contextualTasksApp = document.createElement('contextual-tasks-app');
        document.body.appendChild(contextualTasksApp);

        await microtasksFinished();

        /* Refresh the composebox attribute reference, but not
         * the cr-components composebox. We utilize its wrapper,
         * contextual tasks composebox. */
        const contextualComposebox = contextualTasksApp.$.composebox;
        const header = contextualTasksApp.$.composeboxHeaderWrapper;

        (contextualTasksApp as any).isAiPage_ = false;
        (contextualTasksApp as any).isNavigatingFromAiPage_ = false;

        testProxy.callbackRouterRemote.hideInput();
        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.updateComplete;
        await microtasksFinished();
        assertTrue(
            !!contextualComposebox,
            'Contextual composebox should exist after hideInput');
        assertTrue(!!header, 'Composebox header should exist after hideInput');

        assertTrue(
            header.hidden,
            'Composebox header should be hidden after hideInput');
        assertTrue(
            contextualComposebox.hidden,
            'Contextual composebox should be hidden after hideInput');

        testProxy.callbackRouterRemote.restoreInput();
        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.updateComplete;
        await microtasksFinished();

        assertTrue(
            !!contextualComposebox,
            'Contextual composebox ' +
                'should exist after restoreInput');
        assertFalse(
            contextualComposebox.hidden,
            'Contextual composebox should not be hidden after restoreInput');

        assertTrue(
            !!header,
            'Contextual composebox header should exist after restoreInput');
        assertFalse(
            header.hidden,
            'Composebox header should not be hidden after restoreInput');
      });

  test('Composebox submits then clears input', async () => {
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
        composebox, mockSearchboxPageHandler);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadStarted,
        /*error_type=*/ null,
    );
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessing,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertFalse(submitButton?.disabled, 'Submit button should not be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitButton, 'pointer-events', 'auto',
        'Submit button should not be disabled');
    assertStyle(
        submitContainer, 'cursor', 'pointer',
        'Submit button cursor should be pointer');
    assertTrue(!!submitContainer, 'Submit container button should exist');

    // `submitContainer` must be clickable for tabbing->enter to submit to work.
    submitContainer?.click();

    // Flush the macrotask queue / event loop
    await new Promise(resolve => setTimeout(resolve, 0));

    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(0, composebox.files_.size);

    // Should be no longer `EXPANDING` after successful upload and submit click.
    assertNotEquals(composebox.animationState, GlowAnimationState.EXPANDING);
  });

  test('Composebox submit button enabled for replace files', async () => {
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
        composebox, mockSearchboxPageHandler);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );
    composebox.input_ = 'test';
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton();

    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Submit button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed after first upload');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when disabled after first upload.');

    await composebox.updateComplete;
    await microtasksFinished();

    assertNotEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Query is not submitted via submitQuery_() after first upload');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING, ContextUploadStatus.kUploadReplaced, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(
        0, composebox.getRemainingFilesToUpload().size,
        '0 Files should be uploading');
    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    assertTrue(
        composebox.submitEnabled_,
        'Submit should be enabled after first file upload finishes');
    assertTrue(
        composebox.canSubmitFilesAndInput_,
        'Submit w/files should be enabled after first file upload finishes');

    await uploadFileAndVerify(
        FAKE_TOKEN_STRING_2,
        new File(['foo2'], 'foo2.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler,
        /*expectedInitialFilesCount=*/ 0);
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING_2,
        ContextUploadStatus.kProcessing,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading after second upload starts');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading after second upload starts');

    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Submit button should be disabled');
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed for second upload');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when disabled for second upload.');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING_2,
        ContextUploadStatus.kUploadReplaced,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        0, composebox.getRemainingFilesToUpload().size,
        '0 File should not be uploading after second upload finishes');
    assertTrue(
        composebox.fileUploadsComplete,
        'Files should be finished uploading after second upload finishes');

    // Should be able to submit now that 2nd file is uploaded:
    assertTrue(
        composebox.canSubmitFilesAndInput_,
        'Submit should be enabled after second file upload finishes');

    await composebox.updateComplete;
    await microtasksFinished();
  });

  test('Composebox submit button disabled when uploading files', async () => {
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
        composebox, mockSearchboxPageHandler);
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton();

    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Submit button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when disabled.');

    submitContainer?.click();

    // Flush the macrotask queue / event loop
    await new Promise(resolve => setTimeout(resolve, 0));

    await composebox.updateComplete;
    await microtasksFinished();

    assertNotEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Query is not submitted via submitQuery_()');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await microtasksFinished();
    // Should submit now:
    assertStyle(
        submitContainer, 'cursor', 'pointer',
        'Submit button cursor should be pointer');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when enabled.');

    submitContainer?.click();

    // Flush the macrotask queue / event loop
    await new Promise(resolve => setTimeout(resolve, 0));

    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(0, composebox.files_.size);

    // Should be no longer `EXPANDING` after successful upload and submit click.
    assertNotEquals(composebox.animationState, GlowAnimationState.EXPANDING);
  });

  test('Composebox submit button disabled when uploading tabs', async () => {
    const callback = (file: ComposeboxFile) => {
      composebox.files_.set(file.uuid, file);
      composebox.contextFilesSize_ += 1;
      composebox.submitEnabled_ = composebox.computeSubmitEnabled_();
      composebox.requestUpdate();
    };
    mockSearchboxPageHandler.setResultFor(
        ADD_TAB_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));
    await composebox.addTabContext_({
      detail: {
        id: 0,
        title: 'test',
        url: new URL('https://google.com'),
        delayUpload: false,
        onContextAdded: callback,
      },
    } as CustomEvent);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 tab should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Tabs should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton();

    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Submit button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when disabled.');

    submitContainer?.click();

    // Flush the macrotask queue / event loop
    await new Promise(resolve => setTimeout(resolve, 0));

    await composebox.updateComplete;
    await microtasksFinished();

    assertNotEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Query is not submitted via submitQuery_()');

    // Simulate tab upload success.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );
    await microtasksFinished();
    await composebox.updateComplete;

    assertFalse(submitButton?.disabled, 'Submit button should not be disabled');

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

    assertEquals(0, composebox.files_.size);

    // Should be no longer `EXPANDING` after successful upload and submit click.
    assertNotEquals(composebox.animationState, GlowAnimationState.EXPANDING);
  });

  test('Composebox submits by pressing enter, then clears input', async () => {
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
        composebox, mockSearchboxPageHandler);

    // Other processing state should result in not ready to submit.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertFalse(submitButton?.disabled, 'Submit button should not be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'pointer',
        'Submit button cursor should be pointer');
    assertTrue(!!submitContainer, 'Submit container button should exist');

    pressEnter(submitContainer);
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Query is submitted via submitQuery_()');

    assertEquals(0, composebox.files_.size);
  });

  test('delayed tabs do not delay submission', async () => {
    const callback = (file: any) => {
      composebox.files_.set(file.uuid, file);
      composebox.contextFilesSize_ = 1;
      composebox.submitEnabled_ = composebox.computeSubmitEnabled_();
      composebox.requestUpdate();
    };

    mockSearchboxPageHandler.setResultFor(
        ADD_TAB_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));
    await composebox.addTabContext_({
      detail: {
        id: 0,
        title: 'test',
        url: new URL('https://google.com'),
        delayUpload: true,
        onContextAdded: callback,
      },
    } as CustomEvent);
    await microtasksFinished();
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(
        0, composebox.getRemainingFilesToUpload().size,
        'Delayed tab should have not started uploading');

    assertTrue(
        composebox.fileUploadsComplete,
        'Delayed context should have not started uploading');
    const submitButton: HTMLButtonElement|null = getSubmitButton();

    assertTrue(!!submitButton, 'Submit button should exist');
    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertFalse(
        submitButton?.disabled, 'Submit button should not be disabled!!');

    assertStyle(
        submitButton, 'pointer-events', 'auto',
        'Submit button should not be disabled');
    assertStyle(
        submitContainer, 'cursor', 'pointer',
        'Submit button cursor should be pointer');
    assertTrue(!!submitContainer, 'Submit container button should exist');
  });

  test('Submit button enabled after upload failed', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kProcessing, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(1, composebox.getRemainingFilesToUpload().size);

    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kUploadFailed, null);
    await composebox.updateComplete;
    await composebox.updateComplete;

    await microtasksFinished();
    assertEquals(0, composebox.getRemainingFilesToUpload().size);

    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    // Still disabled until user inputs more text later on.
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');
  });

  test('Submit button enabled after Validation Failed', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kProcessing, null);
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(1, composebox.getRemainingFilesToUpload().size);

    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kValidationFailed, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await microtasksFinished();
    assertEquals(0, composebox.getRemainingFilesToUpload().size);

    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    // Still disabled until user inputs more text later on.
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');
  });

  test('Submit button enabled after file expired', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kProcessing, null);
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(1, composebox.getRemainingFilesToUpload().size);

    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kUploadExpired, null);
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();
    assertEquals(0, composebox.getRemainingFilesToUpload().size);

    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    // Still disabled until user inputs more text later on.
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');
  });

  test('Submit button disabled during Processing', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kProcessingSuggestSignalsReady, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await microtasksFinished();

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');

    assertEquals(1, composebox.getRemainingFilesToUpload().size);
  });
});
