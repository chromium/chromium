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
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {ADD_TAB_CONTEXT_FN, assertStyle, FAKE_TOKEN_STRING, FAKE_TOKEN_STRING_2, fixtureUrl, getSubmitButton, getSubmitContainer, installMock, setupAutocompleteResults, simulateUserInput, uploadFileAndVerify} from './test_utils.js';

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
      enableComposeboxJumpFix: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
    });

    testProxy = new TestContextualTasksBrowserProxy(fixtureUrl);
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getInputState', Promise.resolve({state: new MockInputState()}));

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

    searchboxCallbackRouterRemote.onInputStateChanged(new MockInputState());
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  test('submit enabled when tool is Deep Search', async () => {
    const submitContainer = getSubmitContainer(composebox);
    assertFalse(
        isVisible(submitContainer), 'Submit container should be hidden');

    // Ensure we start in Zero State (disabled).
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await microtasksFinished();

    // Verify submit button is disabled and clicking it does nothing.
    const submitButton = getSubmitButton(composebox);
    assertTrue(submitButton!.disabled, 'Submit button should be disabled');
    submitButton!.click();
    await microtasksFinished();
    assertEquals(
        mockSearchboxPageHandler.getCallCount('submitQuery'), 0,
        'Submit query should not be called when button is disabled');

    // Change tool to Deep Search
    const inputState = Object.assign({}, new MockInputState(), {
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

    const inputElement = composebox.getInputElement().$.input;
    assertTrue(
        isVisible(inputElement), 'Composebox input element should be visible');

    simulateUserInput(inputElement, TEST_QUERY);

    mockTimer.tick(300);

    await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

    await setupAutocompleteResults(
        searchboxCallbackRouterRemote, TEST_QUERY, mockTimer);

    // Wait for the matches to be populated.
    while (!composebox.getDropdownElement().result) {
      mockTimer.tick(10);
      await Promise.resolve();
    }

    pressEnter(inputElement);

    const [matchIndex, url] =
        await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');

    assertEquals(0, matchIndex);
    assertEquals(`${fixtureUrl}/search?q=${TEST_QUERY}`, url);
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
        null, composebox.getDropdownElement().result,
        'Matches should be cleared');
  });

  test('ComposeboxSubmitSendsQueryBeforeAutocomplete', async () => {
    mockTimer.install();
    const TEST_QUERY = 'test query';

    const inputElement = composebox.getInputElement().$.input;
    assertTrue(
        isVisible(inputElement), 'Composebox input element should be visible');

    // User types text
    simulateUserInput(inputElement, TEST_QUERY);
    await composebox.updateComplete;

    // User immediately presses Enter before any autocomplete results arrive
    pressEnter(inputElement);

    // Verify submitQuery is called with the typed text
    const [query] = await mockSearchboxPageHandler.whenCalled('submitQuery');
    assertEquals(TEST_QUERY, query);

    await composebox.updateComplete;
    await contextualTasksApp.updateComplete;

    assertEquals(
        '', inputElement.value,
        'Input should be cleared, but input = ' + inputElement.value);
  });

  test('InjectInputSubmitAfterInjectionTrue', async () => {
    const TEST_QUERY = 'injected query';

    // Call `injectInput` with query text and submit_after_injection = true.
    contextualTasksApp.$.composebox.injectInput({
      title: null,
      thumbnail: null,
      iconId: 0,
      fileToken: null,
      supportsUnimodal: false,
      queryText: TEST_QUERY,
      submitAfterInjection: true,
    });

    // Verify `submitQuery` is called with the injected text.
    const [query] = await mockSearchboxPageHandler.whenCalled('submitQuery');
    assertEquals(TEST_QUERY, query);
  });

  test('InjectInputSubmitAfterInjectionTrueWithFile', async () => {
    const TEST_QUERY = 'injected query';

    contextualTasksApp.$.composebox.injectInput({
      title: 'title',
      thumbnail: 'thumbnail',
      iconId: 0,
      fileToken: FAKE_TOKEN_STRING,
      supportsUnimodal: true,
      queryText: TEST_QUERY,
      submitAfterInjection: true,
    });

    await composebox.updateComplete;

    // Since the file is injected as already successful, it should submit
    // immediately.
    const [query] = await mockSearchboxPageHandler.whenCalled('submitQuery');
    assertEquals(TEST_QUERY, query);
  });

  test('InjectInputSubmitAfterInjectionFalse', async () => {
    const TEST_QUERY = 'injected query';

    // Call `injectInput` with query text and submit_after_injection = false.
    contextualTasksApp.$.composebox.injectInput({
      title: null,
      thumbnail: null,
      iconId: 0,
      fileToken: null,
      supportsUnimodal: false,
      queryText: TEST_QUERY,
      submitAfterInjection: false,
    });
    await composebox.updateComplete;

    // Verify input is set.
    assertEquals(TEST_QUERY, composebox.input);
    assertEquals(TEST_QUERY, composebox.getInputElement().$.input.value);

    // Verify `submitQuery` was not called.
    assertEquals(mockSearchboxPageHandler.getCallCount('submitQuery'), 0);
  });

  test('LensButtonTriggersOverlay', async () => {
    testProxy.handler.setIsShownInTab(false);

    testProxy.callbackRouterRemote.onSidePanelStateChanged();
    await testProxy.callbackRouterRemote.$.flushForTesting();
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
      'hides composebox and header using z-index when enterBasicMode called',
      async () => {
        const threadFrame = contextualTasksApp.$.threadFrame;
        const flexCenterContainer = contextualTasksApp.$.flexCenterContainer;

        testProxy.handler.setIsAiPage(false);
        contextualTasksApp.setIsNavigatingFromAiPageForTesting(false);

        testProxy.callbackRouterRemote.enterBasicMode();
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

        testProxy.callbackRouterRemote.exitBasicMode();
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
      'hides composebox and header when enterBasicMode called' +
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

        testProxy.handler.setIsAiPage(false);
        contextualTasksApp.setIsNavigatingFromAiPageForTesting(false);

        testProxy.callbackRouterRemote.enterBasicMode();
        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.updateComplete;
        await microtasksFinished();
        assertTrue(
            !!contextualComposebox,
            'Contextual composebox should exist after enterBasicMode');
        assertTrue(!!header, 'Composebox header should exist after enterBasicMode');

        assertTrue(
            header.hidden,
            'Composebox header should be hidden after enterBasicMode');
        assertTrue(
            contextualComposebox.hidden,
            'Contextual composebox should be hidden after enterBasicMode');

        testProxy.callbackRouterRemote.exitBasicMode();
        await testProxy.callbackRouterRemote.$.flushForTesting();
        await contextualTasksApp.updateComplete;
        await microtasksFinished();

        assertTrue(
            !!contextualComposebox,
            'Contextual composebox ' +
                'should exist after exitBasicMode');
        assertFalse(
            contextualComposebox.hidden,
            'Contextual composebox should not be hidden after exitBasicMode');

        assertTrue(
            !!header,
            'Contextual composebox header should exist after exitBasicMode');
        assertFalse(
            header.hidden,
            'Composebox header should not be hidden after exitBasicMode');
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
        1, composebox.pendingUploads.size, '1 File should be uploading');
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
        1, composebox.pendingUploads.size, '1 File should be uploading');
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
        1, composebox.pendingUploads.size, '1 File should be uploading');
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

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertFalse(submitButton?.disabled, 'Submit button should not be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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

    assertEquals(0, composebox.files.size);

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
    composebox.input = 'test';
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.pendingUploads.size, '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);

    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Submit button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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
    await composebox.updateComplete;

    assertEquals(
        0, composebox.pendingUploads.size, '0 Files should be uploading');
    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    assertTrue(
        composebox.submitEnabled,
        'Submit should be enabled after first file upload finishes');
    assertTrue(
        composebox.canSubmitFilesAndInput,
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
        1, composebox.pendingUploads.size,
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
        0, composebox.pendingUploads.size,
        '0 File should not be uploading after second upload finishes');
    assertTrue(
        composebox.fileUploadsComplete,
        'Files should be finished uploading after second upload finishes');

    // Should be able to submit now that 2nd file is uploaded:
    assertTrue(
        composebox.canSubmitFilesAndInput,
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
        1, composebox.pendingUploads.size, '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);

    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Submit button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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

    assertEquals(0, composebox.files.size);

    // Should be no longer `EXPANDING` after successful upload and submit click.
    assertNotEquals(composebox.animationState, GlowAnimationState.EXPANDING);
  });

  test('Composebox submit button disabled when uploading tabs', async () => {
    const callback = (file: ComposeboxFile) => {
      composebox.files.set(file.uuid, file);
      composebox.contextFilesSize_ += 1;
      composebox.submitEnabled_ = composebox.computeSubmitEnabled_();
      composebox.requestUpdate();
    };
    mockSearchboxPageHandler.setResultFor(
        ADD_TAB_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));

    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.fire('add-tab-context', {
      id: 0,
      title: 'test',
      url: new URL(fixtureUrl),
      delayUpload: false,
      onContextAdded: callback,
    });
    await microtasksFinished();

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1, composebox.pendingUploads.size, '1 tab should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Tabs should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);

    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Submit button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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

    assertEquals(0, composebox.files.size);

    // Should be no longer `EXPANDING` after successful upload and submit click.
    assertNotEquals(composebox.animationState, GlowAnimationState.EXPANDING);
  });

  test('Composebox submits by pressing enter, then clears input', async () => {
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await microtasksFinished();

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
        1, composebox.pendingUploads.size, '1 File should be uploading');
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

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertFalse(submitButton?.disabled, 'Submit button should not be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'pointer',
        'Submit button cursor should be pointer');
    assertTrue(!!submitContainer, 'Submit container button should exist');

    pressEnter(submitContainer);
    await composebox.updateComplete;
    await microtasksFinished();

    assertNotEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Query is submitted but animation is suppressed on first submit');

    assertEquals(0, composebox.files.size);
  });

  test('Composebox zero state open triggers animation', async () => {
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Opening zero state triggers animation');
  });

  test('Composebox subsequent submit triggers animation', async () => {
    testProxy.callbackRouterRemote.onZeroStateChange(true);
    await microtasksFinished();

    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
        composebox, mockSearchboxPageHandler);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
    assertTrue(!!submitContainer, 'Submit container button should exist');

    // First submit (should not trigger animation).
    pressEnter(submitContainer);
    await composebox.updateComplete;
    await microtasksFinished();

    assertNotEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'First submit suppresses animation');

    await uploadFileAndVerify(
        FAKE_TOKEN_STRING_2,
        new File(['foo2'], 'foo2.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler, /*expectedInitialFilesCount=*/ 0);

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING_2,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );
    testProxy.callbackRouterRemote.onZeroStateChange(false);
    await microtasksFinished();
    await composebox.updateComplete;

    // Second submit!
    pressEnter(submitContainer);
    await composebox.updateComplete;
    await microtasksFinished();

    await new Promise(resolve => requestAnimationFrame(resolve));
    assertEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Subsequent submit triggers animation');
  });

  test('delayed tabs do not delay submission', async () => {
    const callback = (file: any) => {
      composebox.files.set(file.uuid, file);
      composebox.contextFilesSize_ = 1;
      composebox.submitEnabled_ = composebox.computeSubmitEnabled_();
      composebox.requestUpdate();
    };

    mockSearchboxPageHandler.setResultFor(
        ADD_TAB_CONTEXT_FN, Promise.resolve(FAKE_TOKEN_STRING));
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.fire('add-tab-context', {
      id: 0,
      title: 'test',
      url: new URL(fixtureUrl),
      delayUpload: true,
      onContextAdded: callback,
    });
    await microtasksFinished();
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(
        0, composebox.pendingUploads.size,
        'Delayed tab should have not started uploading');

    assertTrue(
        composebox.fileUploadsComplete,
        'Delayed context should have not started uploading');
    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);

    assertTrue(!!submitButton, 'Submit button should exist');
    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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

    assertEquals(1, composebox.pendingUploads.size);

    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    assertEquals(0, composebox.pendingUploads.size);

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
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;

    assertEquals(1, composebox.pendingUploads.size);

    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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
    assertEquals(0, composebox.pendingUploads.size);

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
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;

    assertEquals(1, composebox.pendingUploads.size);

    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
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
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    assertEquals(0, composebox.pendingUploads.size);

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

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer(composebox);
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
              even when disabled.');

    assertEquals(1, composebox.pendingUploads.size);
  });

  test('Submit button disabled if no input supports unimodal', async () => {
    composebox.injectInput(
        'title', 'thumbnail', FAKE_TOKEN_STRING, /*supportsUnimodal=*/ false);
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;

    assertEquals(
        0, composebox.pendingUploads.size, 'pendingUploads should be 0');
    assertTrue(composebox.submitEnabled, 'submitEnabled should be true');
    assertFalse(
        composebox.canSubmitFilesAndInput,
        'canSubmitFilesAndInput should be false');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');
  });

  test(
      'Submit button enabled if no input supports unimodal but has text query',
      async () => {
        composebox.injectInput(
            'title', 'thumbnail', FAKE_TOKEN_STRING,
            /*supportsUnimodal=*/ false);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful, null);
        composebox.input = 'test';

        await searchboxCallbackRouterRemote.$.flushForTesting();
        await composebox.updateComplete;

        const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
        assertTrue(!!submitButton, 'Submit button should exist');
        assertFalse(submitButton?.disabled, 'Button should be enabled');
      });

  test('Submit button enabled if input supports unimodal', async () => {
    composebox.injectInput(
        'title', 'thumbnail', FAKE_TOKEN_STRING, /*supportsUnimodal=*/ true);
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;

    assertEquals(
        0, composebox.pendingUploads.size, 'pendingUploads should be 0');
    assertTrue(composebox.submitEnabled, 'submitEnabled should be true');
    assertTrue(
        composebox.canSubmitFilesAndInput,
        'canSubmitFilesAndInput should be true');

    const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
    assertTrue(!!submitButton, 'Submit button should exist');
    assertFalse(submitButton?.disabled, 'Button should be enabled');
  });

  test(
      'Submit button enabled if at least one input supports unimodal',
      async () => {
        composebox.injectInput(
            'title', 'thumbnail', FAKE_TOKEN_STRING,
            /*supportsUnimodal=*/ false);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful, null);
        composebox.injectInput(
            'title2', 'thumbnail2', FAKE_TOKEN_STRING_2,
            /*supportsUnimodal=*/ true);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING_2, ContextUploadStatus.kUploadSuccessful, null);

        await searchboxCallbackRouterRemote.$.flushForTesting();
        await composebox.updateComplete;

        const submitButton: HTMLButtonElement|null = getSubmitButton(composebox);
        assertTrue(!!submitButton, 'Submit button should exist');
        assertFalse(submitButton?.disabled, 'Button should be enabled');
      });
});
