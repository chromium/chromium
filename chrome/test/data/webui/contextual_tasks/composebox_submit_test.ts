// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For composebox tests related to tools, secondary inputs (voice, drag/drop).
import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {IconType} from 'chrome://contextual-tasks/contextual_tasks.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextUploadStatus, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteResult, PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockInputState} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, createCtComposeboxApp, deleteLastFile, FAKE_TOKEN_STRING, FAKE_TOKEN_STRING_2, fixtureUrl, getInputValue, getSubmitButton, getSubmitContainer, installMock, simulateUserInput} from './contextual_tasks_test_utils.js';
import type {CtComposeboxAppParts} from './contextual_tasks_test_utils.js';
import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {ADD_TAB_CONTEXT_FN, setupAutocompleteResults, uploadFileAndVerify} from './test_searchbox_utils.js';

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
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
      ComposeboxPageHandlerRemote;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
      SearchboxPageHandlerRemote;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let windowProxy: TestMock<WindowProxy>;
  let mockTimer: MockTimer;

  setup(async () => {
    const win = window as unknown as {chrome: any, trustedTypes: any};

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
      useContextualTasksComposeboxFork: false,
      contextualMenuUsePecApi: false,
      composeboxSmartTabSharingVisible: false,
      contextManagementInComposeboxEnabled: false,
      enableComposeboxJumpFix: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
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
        'getInputState', Promise.resolve({state: new MockInputState()}));
    mockSearchboxPageHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler, mockSearchboxPageHandler,
        searchboxCallbackRouter));

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

    assertEquals(0, composebox.attachedContext.size);

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

    assertEquals(0, composebox.attachedContext.size);

    // Should be no longer `EXPANDING` after successful upload and submit click.
    assertNotEquals(composebox.animationState, GlowAnimationState.EXPANDING);
  });

  test('Composebox submit button disabled when uploading tabs', async () => {
    const callback = (file: ComposeboxFile) => {
      composebox.attachedContext.set(file.uuid, file);
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

    assertEquals(0, composebox.attachedContext.size);

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

    assertEquals(0, composebox.attachedContext.size);
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
      composebox.attachedContext.set(file.uuid, file);
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
});

// =============================================================================
// Fork DUAL-PATH SUBMIT SUITE
// Submit behavior - both submit-before-autocomplete and selected-match submit -
// is implemented by both the legacy <cr-composebox> and
// the <contextual-tasks-inner-composebox>, so this suite runs on both paths.
// Submit tests depending on behavior the fork does not implement yet
// (files) stay in the flag-off suites above.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkSubmitTest (useContextualTasksComposeboxFork =
        ${useFork})`,
      () => {
        let testProxy: TestContextualTasksBrowserProxy;
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let searchboxCallbackRouterRemote: SearchboxPageRemote;
        let parts: CtComposeboxAppParts;
        let mockTimer: MockTimer;

        setup(async () => {
          const win = window as unknown as {chrome: any, trustedTypes: any};

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

          document.body.innerHTML = win.trustedTypes!.emptyHTML;

          mockTimer = new MockTimer();

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
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
              'getInputState', Promise.resolve({state: new MockInputState()}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'addTabContext',
              Promise.resolve({high: BigInt(1), low: BigInt(2)}));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouterRemote =
              searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
        });

        teardown(() => {
          mockTimer.uninstall();
        });

        test('ComposeboxSubmitSendsQueryBeforeAutocomplete', async () => {
          mockTimer.install();
          const TEST_QUERY = 'test query';
          const {app, innerComposebox} = parts;

          const inputElement = innerComposebox.getInputElement().$.input;
          assertTrue(
              isVisible(inputElement),
              'Composebox input element should be visible');

          // User types text
          simulateUserInput(inputElement, TEST_QUERY);
          await innerComposebox.updateComplete;

          // User immediately presses Enter before any autocomplete results
          // arrive
          pressEnter(inputElement);

          // Verify submitQuery is called with the typed text
          const [query] =
              await mockSearchboxPageHandler.whenCalled('submitQuery');
          assertEquals(TEST_QUERY, query);

          await innerComposebox.updateComplete;
          await app.updateComplete;

          assertEquals(
              '', getInputValue(inputElement),
              'Input should be cleared, but input = ' +
                  getInputValue(inputElement));
        });

        test('ComposeboxSubmitSendsQueryAndClearsInput', async () => {
          mockTimer.install();
          const TEST_QUERY = 'test query';
          const {app, innerComposebox} = parts;

          const inputElement = innerComposebox.getInputElement().$.input;
          assertTrue(
              isVisible(inputElement),
              'Composebox input element should be visible');

          simulateUserInput(inputElement, TEST_QUERY);
          mockTimer.tick(300);

          await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

          await setupAutocompleteResults(
              searchboxCallbackRouterRemote, innerComposebox.activeQueryId,
              TEST_QUERY, mockTimer);

          // Wait for the matches to be populated.
          while (!innerComposebox.getDropdownElement().result) {
            mockTimer.tick(10);
            await Promise.resolve();
          }

          pressEnter(inputElement);

          const [matchIndex, url] = await mockSearchboxPageHandler.whenCalled(
              'openAutocompleteMatch');
          assertEquals(0, matchIndex);
          assertEquals(`${fixtureUrl}/search?q=${TEST_QUERY}`, url);
          mockTimer.tick(0);

          await innerComposebox.updateComplete;
          await app.updateComplete;

          assertEquals(
              '', getInputValue(inputElement),
              'Input should be cleared, but input = ' +
                  getInputValue(inputElement));
          assertEquals(
              null, innerComposebox.getDropdownElement().result,
              'Matches should be cleared');
        });

        test('empty query submits for Deep Search follow-up', async () => {
          const {app, innerComposebox} = parts;

          // Zero state with no tool: an empty query is not submittable.
          testProxy.callbackRouterRemote.onZeroStateChange(true);
          await microtasksFinished();
          await app.updateComplete;
          await innerComposebox.updateComplete;
          assertFalse(
              innerComposebox.canSubmitFilesAndInput,
              'Empty query in zero state is not submittable');

          // Deep Search while still in zero state: still not submittable
          const deepSearchState = Object.assign({}, new MockInputState(), {
            activeTool: ToolMode.kDeepSearch,
          });
          searchboxCallbackRouterRemote.onInputStateChanged(deepSearchState);
          await searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();
          await innerComposebox.updateComplete;
          assertFalse(
              innerComposebox.canSubmitFilesAndInput,
              'Deep Search in zero state does not allow an empty query');

          // The submit button is rendered but disabled, and clicking it does
          // not send the query.
          const disabledButton = getSubmitButton(innerComposebox);
          assertTrue(disabledButton !== null, 'Submit button should exist');
          assertTrue(
              disabledButton.disabled, 'Submit button should be disabled');
          const submitCountWhileDisabled =
              mockSearchboxPageHandler.getCallCount('submitQuery');
          disabledButton.click();
          await microtasksFinished();
          await innerComposebox.updateComplete;
          assertEquals(
              submitCountWhileDisabled,
              mockSearchboxPageHandler.getCallCount('submitQuery'),
              'Clicking the disabled submit button should be a no-op');

          // Deep Search follow-up (not zero state): now submittable.
          testProxy.callbackRouterRemote.onZeroStateChange(false);
          await microtasksFinished();
          await app.updateComplete;
          await innerComposebox.updateComplete;
          assertTrue(
              innerComposebox.canSubmitFilesAndInput,
              'Deep Search follow-up allows an empty query');

          // The submit button is present, enabled, and clicking it actually
          // sends the (empty) query once.
          const submitButton = getSubmitButton(innerComposebox);
          assertTrue(submitButton !== null, 'Submit button should exist');
          assertFalse(submitButton.disabled, 'Submit button should be enabled');
          const submitCountBefore =
              mockSearchboxPageHandler.getCallCount('submitQuery');
          submitButton.click();
          await microtasksFinished();
          await innerComposebox.updateComplete;
          assertEquals(
              submitCountBefore + 1,
              mockSearchboxPageHandler.getCallCount('submitQuery'),
              'Clicking the submit button should send the empty query once');
        });

        test('empty query blocked for non-Deep-Search follow-up', async () => {
          const {app, innerComposebox} = parts;

          // A non-Deep-Search tool during a follow-up must not allow an empty
          // query.
          const noToolState = Object.assign({}, new MockInputState(), {
            activeTool: ToolMode.kUnspecified,
          });
          searchboxCallbackRouterRemote.onInputStateChanged(noToolState);
          await searchboxCallbackRouterRemote.$.flushForTesting();
          testProxy.callbackRouterRemote.onZeroStateChange(false);
          await microtasksFinished();
          await app.updateComplete;
          await innerComposebox.updateComplete;
          assertTrue(
              innerComposebox.isFollowupQuery,
              'Follow-up state should have propagated');
          assertFalse(
              innerComposebox.canSubmitFilesAndInput,
              'Non-Deep-Search follow-up does not allow an empty query');
        });

        test('inner composebox reflects the expanding_ attribute', async () => {
          const {innerComposebox} = parts;
          await innerComposebox.updateComplete;
          assertTrue(
              innerComposebox.hasAttribute('expanding_'),
              'The inner composebox host should reflect [expanding_]');
        });

        test('expanded host is not styled as collapsed', async () => {
          const {app, innerComposebox} = parts;
          // Skip the icon-fade transitions (600ms plus a 200ms delay), so the
          // computed styles below read their end values.
          disableAnimationsRecursively(app);
          const inputComponent = innerComposebox.getInputElement();
          simulateUserInput(inputComponent.$.input, 'test query');
          await microtasksFinished();
          await innerComposebox.updateComplete;
          await inputComponent.updateComplete;

          const submitContainer = getSubmitContainer(innerComposebox);
          assertTrue(submitContainer !== null, 'Submit container should exist');
          const cancelContainer =
              inputComponent.shadowRoot.querySelector('#cancelContainer');
          assertTrue(cancelContainer !== null, 'Cancel container should exist');

          // The icon-fade containers are opaque only when a composed ancestor
          // reflects [expanding_].
          assertStyle(
              submitContainer, 'opacity', '1',
              'Submit container should be visible on the expanded host');
          assertStyle(
              cancelContainer, 'opacity', '1',
              'Cancel container should be visible on the expanded host');

          // The expanded host must not match the wrapper's collapsed-state
          // `:not([expanding_])` rules that disable the parts' pointer events.
          assertStyle(
              submitContainer, 'pointer-events', 'auto',
              'Expanded host must not match the collapsed-state selector');
          assertStyle(
              cancelContainer, 'pointer-events', 'auto',
              'Expanded host must not match the collapsed-state selector');
        });
      });
});

// =============================================================================
// Fork DUAL-PATH INJECT-INPUT SUITE
// Programmatic injection - injectInput / setInputProgrammatically /
// submitAfterInjection - is implemented by both the legacy <cr-composebox>
// and the <contextual-tasks-inner-composebox>, so this suite runs on both
// paths.
// =============================================================================
[true, false].forEach(useFork => {
  suite(
      `ContextualTasksComposeboxForkInjectInputTest ` +
          `(useContextualTasksComposeboxFork = ${useFork})`,
      () => {
        const QUERY_AUTOCOMPLETE_FN = 'queryAutocomplete';
        let testProxy: TestContextualTasksBrowserProxy;
        let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>&
            ComposeboxPageHandlerRemote;
        let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>&
            SearchboxPageHandlerRemote;
        let searchboxCallbackRouterRemote: SearchboxPageRemote;
        let parts: CtComposeboxAppParts;

        setup(async () => {
          const win = window as unknown as {chrome: any, trustedTypes: any};

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

          document.body.innerHTML = win.trustedTypes!.emptyHTML;

          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
            composeboxSmartTabSharingVisible: false,
            enableComposeboxJumpFix: false,
            composeboxShowTypedSuggest: true,
            composeboxShowZps: true,
            enableBasicModeZOrder: true,
            composeboxShowContextMenu: true,
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
              'getInputState', Promise.resolve({state: new MockInputState()}));
          mockSearchboxPageHandler.setResultFor(
              'getPageClassification',
              Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
          mockSearchboxPageHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: []}));
          mockSearchboxPageHandler.setResultFor(
              'addTabContext',
              Promise.resolve({high: BigInt(1), low: BigInt(2)}));
          const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
          searchboxCallbackRouterRemote =
              searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
          ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
              mockComposeboxPageHandler, mockSearchboxPageHandler,
              searchboxCallbackRouter));

          parts = await createCtComposeboxApp(useFork);
        });

        // Flushes pending searchbox callbacks into the page, then settles the
        // inner element and the wrapper so reactive state is current.
        async function flushAndSettle() {
          await searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();
          await parts.innerComposebox.updateComplete;
          await parts.wrapper.updateComplete;
        }

        // Reads the wrapper's protected suggestions state (test-only cast).
        function wrapperSuggestionsState() {
          return parts.wrapper as unknown as {
            zeroStateSuggestions_: AutocompleteResult,
            isLoading_: boolean,
          };
        }

        // Creates a real pending upload: the file-add surface registers the
        // file, and a non-terminal processing status for the same token moves
        // it into `pendingUploads`.
        async function createPendingUpload() {
          const {innerComposebox} = parts;
          await uploadFileAndVerify(
              FAKE_TOKEN_STRING,
              new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
              innerComposebox, mockSearchboxPageHandler);
          searchboxCallbackRouterRemote.onContextualInputStatusChanged(
              FAKE_TOKEN_STRING, ContextUploadStatus.kProcessing, null);
          await flushAndSettle();
          assertTrue(
              innerComposebox.attachedContext.get(FAKE_TOKEN_STRING) !==
                  undefined,
              'The pending file should be tracked in `attachedContext`');
          assertFalse(
              innerComposebox.fileUploadsComplete,
              'The processing upload should be pending');
          assertFalse(
              innerComposebox.canSubmitFilesAndInput,
              'A pending upload should block submission');
        }

        test('InjectInputSubmitAfterInjectionTrue', async () => {
          const {innerComposebox, wrapper} = parts;
          const TEST_QUERY = 'injected query';
          const submitBaseline =
              mockSearchboxPageHandler.getCallCount('submitQuery');

          // Call `injectInput` with query text and submit_after_injection =
          // true.
          await wrapper.injectInput({
            title: null,
            thumbnail: null,
            iconId: IconType.kUnspecified,
            fileToken: null,
            supportsUnimodal: false,
            queryText: TEST_QUERY,
            submitAfterInjection: true,
          });
          await innerComposebox.updateComplete;

          // Verify `submitQuery` is called once with the injected text.
          assertEquals(
              submitBaseline + 1,
              mockSearchboxPageHandler.getCallCount('submitQuery'),
              'The injection should submit exactly once');
          const submitArgs = mockSearchboxPageHandler.getArgs('submitQuery');
          assertEquals(TEST_QUERY, submitArgs[submitArgs.length - 1][0]);
        });

        test('InjectInputSubmitAfterInjectionTrueWithFile', async () => {
          const {innerComposebox, wrapper} = parts;
          const TEST_QUERY = 'injected query';
          const submitBaseline =
              mockSearchboxPageHandler.getCallCount('submitQuery');

          await wrapper.injectInput({
            title: 'title',
            thumbnail: 'thumbnail',
            iconId: IconType.kUnspecified,
            fileToken: FAKE_TOKEN_STRING,
            supportsUnimodal: true,
            queryText: TEST_QUERY,
            submitAfterInjection: true,
          });
          await innerComposebox.updateComplete;

          // Since the file is injected as already successful, it should submit
          // immediately.
          assertEquals(
              submitBaseline + 1,
              mockSearchboxPageHandler.getCallCount('submitQuery'),
              'The injection should submit exactly once');
          const submitArgs = mockSearchboxPageHandler.getArgs('submitQuery');
          assertEquals(TEST_QUERY, submitArgs[submitArgs.length - 1][0]);
        });

        test('InjectInputSubmitAfterInjectionFalse', async () => {
          const {innerComposebox, wrapper} = parts;
          const TEST_QUERY = 'injected query';
          const submitBaseline =
              mockSearchboxPageHandler.getCallCount('submitQuery');

          // Call `injectInput` with query text and submit_after_injection =
          // false.
          await wrapper.injectInput({
            title: null,
            thumbnail: null,
            iconId: IconType.kUnspecified,
            fileToken: null,
            supportsUnimodal: false,
            queryText: TEST_QUERY,
            submitAfterInjection: false,
          });
          await innerComposebox.updateComplete;
          await innerComposebox.getInputElement().updateComplete;

          // Verify input is set.
          assertEquals(TEST_QUERY, innerComposebox.input);
          assertEquals(
              TEST_QUERY,
              getInputValue(innerComposebox.getInputElement().$.input));

          // Verify `submitQuery` was not called.
          assertEquals(
              submitBaseline,
              mockSearchboxPageHandler.getCallCount('submitQuery'));
        });

        test('InjectInputPopulatesInjectedFileFields', async () => {
          const {innerComposebox, wrapper} = parts;
          // Contains a space, ':', '/', '?' and '&' so the encoded URL below
          // differs from plain concatenation.
          const THUMBNAIL = 'https://example.com/a b.png?x=1&y=2';

          await wrapper.injectInput({
            title: 'injected title',
            thumbnail: THUMBNAIL,
            iconId: IconType.kFormatQuoteFilled,
            fileToken: FAKE_TOKEN_STRING,
            supportsUnimodal: true,
            queryText: null,
            submitAfterInjection: false,
          });
          await innerComposebox.updateComplete;

          const file = innerComposebox.attachedContext.get(FAKE_TOKEN_STRING);
          assertTrue(file !== undefined, 'The injected file should exist');
          assertEquals(FAKE_TOKEN_STRING, file.uuid);
          assertEquals('injected title', file.name);
          assertEquals('injectedinput', file.type);
          assertEquals(ContextUploadStatus.kUploadSuccessful, file.status);
          const expectedUrl =
              'chrome://image?url=' + encodeURIComponent(THUMBNAIL);
          assertEquals(expectedUrl, file.dataUrl);
          assertEquals(expectedUrl, file.objectUrl);
          assertEquals('quoteFilled', file.iconName);
          assertTrue(file.supportsUnimodal);
        });

        test('LateAutocompleteResultsGatedByQueryId', async () => {
          const {innerComposebox, wrapper} = parts;
          const TEST_QUERY = 'injected query';

          const queryBaseline =
              mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN);
          await wrapper.injectInput({
            title: null,
            thumbnail: null,
            iconId: IconType.kUnspecified,
            fileToken: null,
            supportsUnimodal: false,
            queryText: TEST_QUERY,
            submitAfterInjection: false,
          });
          await innerComposebox.updateComplete;

          // Anchor: the non-submit injection queried autocomplete once for
          // the injected text.
          assertEquals(
              queryBaseline + 1,
              mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN),
              'The non-submit injection should query autocomplete once');
          const queryArgs =
              mockSearchboxPageHandler.getArgs(QUERY_AUTOCOMPLETE_FN);
          assertEquals(TEST_QUERY, queryArgs[queryArgs.length - 1][2]);
          const activeQueryId = innerComposebox.activeQueryId;
          assertTrue(activeQueryId >= 0, 'A live query id should be active');

          const resultChangedResults: AutocompleteResult[] = [];
          innerComposebox.addEventListener('result-changed', e => {
            resultChangedResults.push(
                (e as CustomEvent<AutocompleteResult>).detail);
          });

          // A stale-ID result whose input matches `lastQueriedInput`: only
          // the query-ID guard can reject it.
          const wrapperState = wrapperSuggestionsState();
          const suggestionsBefore = {
            queryId: wrapperState.zeroStateSuggestions_.queryId,
            input: wrapperState.zeroStateSuggestions_.input,
            matchCount: wrapperState.zeroStateSuggestions_.matches.length,
          };
          wrapper.setIsLoadingForTesting(true);
          searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                queryId: activeQueryId + 1,
                input: TEST_QUERY,
                matches: [createAutocompleteMatch({
                  allowedToBeDefaultMatch: false,
                  contents: TEST_QUERY,
                })],
              }));
          await flushAndSettle();

          assertEquals(TEST_QUERY, innerComposebox.input);
          assertEquals(null, innerComposebox.result);
          assertEquals(null, innerComposebox.getDropdownElement().result);
          assertEquals(
              0, resultChangedResults.length,
              'A stale-ID result must not fire result-changed');
          assertEquals(
              suggestionsBefore.queryId,
              wrapperState.zeroStateSuggestions_.queryId);
          assertEquals(
              suggestionsBefore.input,
              wrapperState.zeroStateSuggestions_.input);
          assertEquals(
              suggestionsBefore.matchCount,
              wrapperState.zeroStateSuggestions_.matches.length);
          assertTrue(
              wrapperState.isLoading_,
              'A rejected result must not touch the wrapper loading state');

          // A correct-ID result with a DIFFERENT input is accepted end to end:
          // mixin state, the outward result-changed event, and the wrapper
          // suggestions.
          const OTHER_QUERY = TEST_QUERY + ' refined';
          searchboxCallbackRouterRemote.autocompleteResultChanged(
              createAutocompleteResultForTesting({
                queryId: activeQueryId,
                input: OTHER_QUERY,
                matches: [
                  createAutocompleteMatch({
                    allowedToBeDefaultMatch: false,
                    contents: OTHER_QUERY,
                  }),
                  createAutocompleteMatch({allowedToBeDefaultMatch: false}),
                ],
              }));
          await flushAndSettle();

          const accepted = innerComposebox.result;
          assertTrue(accepted !== null, 'The result should be accepted');
          assertEquals(activeQueryId, accepted.queryId);
          assertEquals(OTHER_QUERY, accepted.input);
          assertEquals(2, accepted.matches.length);
          assertEquals(
              1, resultChangedResults.length,
              'The accepted result must fire result-changed once');
          const detail = resultChangedResults[0]!;
          assertEquals(activeQueryId, detail.queryId);
          assertEquals(OTHER_QUERY, detail.input);
          assertEquals(2, detail.matches.length);
          assertEquals(
              activeQueryId, wrapperState.zeroStateSuggestions_.queryId);
          assertEquals(OTHER_QUERY, wrapperState.zeroStateSuggestions_.input);
          assertEquals(2, wrapperState.zeroStateSuggestions_.matches.length);
          assertFalse(
              wrapperState.isLoading_,
              'The accepted result should clear the wrapper loading state');
        });

        test(
            'LateResultRejectedWhileSubmitDeferredOnPendingUpload',
            async () => {
              const {innerComposebox, wrapper} = parts;
              const NEW_QUERY = 'injected submit query';

              await createPendingUpload();

              // A real ZPS request whose id the late result replays below.
              const queryBaseline =
                  mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN);
              await wrapper.injectInput({
                title: null,
                thumbnail: null,
                iconId: IconType.kUnspecified,
                fileToken: null,
                supportsUnimodal: false,
                queryText: '',
                submitAfterInjection: false,
              });
              await innerComposebox.updateComplete;
              assertEquals(
                  queryBaseline + 1,
                  mockSearchboxPageHandler.getCallCount(QUERY_AUTOCOMPLETE_FN),
                  'The empty injection should issue one ZPS query');
              const queryArgs =
                  mockSearchboxPageHandler.getArgs(QUERY_AUTOCOMPLETE_FN);
              assertEquals('', queryArgs[queryArgs.length - 1][2]);
              const staleQueryId = innerComposebox.activeQueryId;
              assertTrue(staleQueryId >= 0, 'The ZPS query should be live');

              // Inject the query to submit. The pending upload defers the
              // wrapper submit, so `submitting` stays false and only the
              // query-ID guard can reject the late result below.
              const stopBaseline =
                  mockSearchboxPageHandler.getCallCount('stopAutocomplete');
              const submitBaseline =
                  mockSearchboxPageHandler.getCallCount('submitQuery');
              await wrapper.injectInput({
                title: null,
                thumbnail: null,
                iconId: IconType.kUnspecified,
                fileToken: null,
                supportsUnimodal: false,
                queryText: NEW_QUERY,
                submitAfterInjection: true,
              });
              await innerComposebox.updateComplete;
              assertEquals(NEW_QUERY, innerComposebox.input);
              assertEquals(-1, innerComposebox.activeQueryId);
              assertEquals('', innerComposebox.lastQueriedInput);
              assertEquals(
                  submitBaseline,
                  mockSearchboxPageHandler.getCallCount('submitQuery'),
                  'The submit should be deferred on the pending upload');
              assertEquals(
                  stopBaseline + 2,
                  mockSearchboxPageHandler.getCallCount('stopAutocomplete'),
                  'The submit-path injection stops autocomplete twice: the ' +
                      'explicit stop plus clearAutocompleteMatches');
              assertEquals(null, innerComposebox.result);
              assertEquals(null, innerComposebox.getDropdownElement().result);
              assertFalse(
                  innerComposebox.submitting,
                  'The deferred submit must not set `submitting`, so only ' +
                      'the query-ID guard can reject the late result below');

              // Replay a result for the pre-injection query id with
              // input === '' (=== lastQueriedInput), so an input-based guard
              // would wrongly accept it.
              let resultChangedCount = 0;
              innerComposebox.addEventListener(
                  'result-changed', () => resultChangedCount++);
              const wrapperState = wrapperSuggestionsState();
              const suggestionsBefore = {
                queryId: wrapperState.zeroStateSuggestions_.queryId,
                input: wrapperState.zeroStateSuggestions_.input,
                matchCount: wrapperState.zeroStateSuggestions_.matches.length,
              };
              wrapper.setIsLoadingForTesting(true);
              searchboxCallbackRouterRemote.autocompleteResultChanged(
                  createAutocompleteResultForTesting({
                    queryId: staleQueryId,
                    input: '',
                    matches: [createAutocompleteMatch(
                        {allowedToBeDefaultMatch: false})],
                  }));
              await flushAndSettle();

              assertEquals(NEW_QUERY, innerComposebox.input);
              assertEquals(null, innerComposebox.result);
              assertEquals(null, innerComposebox.getDropdownElement().result);
              assertEquals(
                  0, resultChangedCount,
                  'The stale result must not fire result-changed');
              assertEquals(
                  suggestionsBefore.queryId,
                  wrapperState.zeroStateSuggestions_.queryId);
              assertEquals(
                  suggestionsBefore.input,
                  wrapperState.zeroStateSuggestions_.input);
              assertEquals(
                  suggestionsBefore.matchCount,
                  wrapperState.zeroStateSuggestions_.matches.length);
              assertTrue(
                  wrapperState.isLoading_,
                  'The stale result must not touch the wrapper loading state');
              assertEquals(
                  submitBaseline,
                  mockSearchboxPageHandler.getCallCount('submitQuery'),
                  'The submit should still be deferred');

              // Completing the upload sends the injected query exactly once.
              searchboxCallbackRouterRemote.onContextualInputStatusChanged(
                  FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful,
                  null);
              await flushAndSettle();
              assertEquals(
                  submitBaseline + 1,
                  mockSearchboxPageHandler.getCallCount('submitQuery'),
                  'Completing the upload should submit exactly once');
              const submitArgs =
                  mockSearchboxPageHandler.getArgs('submitQuery');
              assertEquals(NEW_QUERY, submitArgs[submitArgs.length - 1][0]);
            });

        test('InjectInputSubmitDeferredUntilUploadCompletes', async () => {
          const {innerComposebox, wrapper} = parts;
          const TEST_QUERY = 'injected query';

          await createPendingUpload();

          const submitBaseline =
              mockSearchboxPageHandler.getCallCount('submitQuery');
          await wrapper.injectInput({
            title: null,
            thumbnail: null,
            iconId: IconType.kUnspecified,
            fileToken: null,
            supportsUnimodal: false,
            queryText: TEST_QUERY,
            submitAfterInjection: true,
          });
          await innerComposebox.updateComplete;
          assertEquals(
              submitBaseline,
              mockSearchboxPageHandler.getCallCount('submitQuery'),
              'The submit should be deferred on the pending upload');

          // Completing the upload drives the real
          // can-submit-files-and-input-changed chain to the deferred submit.
          searchboxCallbackRouterRemote.onContextualInputStatusChanged(
              FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful, null);
          await flushAndSettle();
          assertEquals(
              submitBaseline + 1,
              mockSearchboxPageHandler.getCallCount('submitQuery'),
              'Completing the upload should submit exactly once');
          const submitArgs = mockSearchboxPageHandler.getArgs('submitQuery');
          assertEquals(TEST_QUERY, submitArgs[submitArgs.length - 1][0]);
        });

        test(
            'Submit button disabled if no input supports unimodal',
            async () => {
              const {innerComposebox} = parts;
              innerComposebox.injectInput(
                  'title', 'thumbnail', FAKE_TOKEN_STRING,
                  /*supportsUnimodal=*/ false);
              searchboxCallbackRouterRemote.onContextualInputStatusChanged(
                  FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful,
                  null);
              await flushAndSettle();

              assertEquals(
                  0, innerComposebox.pendingUploads.size,
                  'pendingUploads should be 0');
              assertTrue(
                  innerComposebox.submitEnabled,
                  'submitEnabled should be true');
              assertFalse(
                  innerComposebox.canSubmitFilesAndInput,
                  'canSubmitFilesAndInput should be false');

              const submitButton = getSubmitButton(innerComposebox);
              assertTrue(submitButton !== null, 'Submit button should exist');
              assertTrue(submitButton.disabled, 'Button should be disabled');
            });

        test(
            'Submit button enabled if no input supports unimodal but has ' +
                'text query',
            async () => {
              const {innerComposebox} = parts;
              innerComposebox.injectInput(
                  'title', 'thumbnail', FAKE_TOKEN_STRING,
                  /*supportsUnimodal=*/ false);
              searchboxCallbackRouterRemote.onContextualInputStatusChanged(
                  FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful,
                  null);
              innerComposebox.input = 'test';
              await flushAndSettle();

              // Anchor the injected file: text alone enables the button, so
              // a no-op injectInput would otherwise pass this test.
              const file =
                  innerComposebox.attachedContext.get(FAKE_TOKEN_STRING);
              assertTrue(file !== undefined, 'The injected file should exist');
              assertFalse(
                  file.supportsUnimodal,
                  'The injected file must not support unimodal');

              const submitButton = getSubmitButton(innerComposebox);
              assertTrue(submitButton !== null, 'Submit button should exist');
              assertFalse(submitButton.disabled, 'Button should be enabled');
            });

        test('Submit button enabled if input supports unimodal', async () => {
          const {innerComposebox} = parts;
          innerComposebox.injectInput(
              'title', 'thumbnail', FAKE_TOKEN_STRING,
              /*supportsUnimodal=*/ true);
          searchboxCallbackRouterRemote.onContextualInputStatusChanged(
              FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful, null);
          await flushAndSettle();

          assertEquals(
              0, innerComposebox.pendingUploads.size,
              'pendingUploads should be 0');
          assertTrue(
              innerComposebox.submitEnabled, 'submitEnabled should be true');
          assertTrue(
              innerComposebox.canSubmitFilesAndInput,
              'canSubmitFilesAndInput should be true');

          const submitButton = getSubmitButton(innerComposebox);
          assertTrue(submitButton !== null, 'Submit button should exist');
          assertFalse(submitButton.disabled, 'Button should be enabled');
        });

        test(
            'Submit button enabled if at least one input supports unimodal',
            async () => {
              const {innerComposebox} = parts;
              innerComposebox.injectInput(
                  'title', 'thumbnail', FAKE_TOKEN_STRING,
                  /*supportsUnimodal=*/ false);
              searchboxCallbackRouterRemote.onContextualInputStatusChanged(
                  FAKE_TOKEN_STRING, ContextUploadStatus.kUploadSuccessful,
                  null);
              innerComposebox.injectInput(
                  'title2', 'thumbnail2', FAKE_TOKEN_STRING_2,
                  /*supportsUnimodal=*/ true);
              searchboxCallbackRouterRemote.onContextualInputStatusChanged(
                  FAKE_TOKEN_STRING_2, ContextUploadStatus.kUploadSuccessful,
                  null);
              await flushAndSettle();

              const submitButton = getSubmitButton(innerComposebox);
              assertTrue(submitButton !== null, 'Submit button should exist');
              assertFalse(submitButton.disabled, 'Button should be enabled');
            });

        test('Injected input can be added, then deleted from AIM', async () => {
          const {innerComposebox} = parts;
          innerComposebox.injectInput(
              'title', 'thumbnail.jpg', FAKE_TOKEN_STRING,
              /*supportsUnimodal=*/ false);
          await innerComposebox.updateComplete;
          await microtasksFinished();

          // Avoid using $.carousel since it may be cached.
          const carousel =
              innerComposebox.shadowRoot
                  .querySelector<ComposeboxFileCarouselElement>('#carousel');
          assertTrue(carousel !== null, 'Carousel should be in the DOM');
          assertEquals(1, carousel.files.length);

          innerComposebox.deleteFile(FAKE_TOKEN_STRING);
          await innerComposebox.updateComplete;
          await microtasksFinished();
          assertEquals(
              null, innerComposebox.shadowRoot.querySelector('#carousel'),
              'Carousel should be removed from the DOM');
        });

        test(
            'Injected input with icon can be added, then deleted from AIM',
            async () => {
              const {innerComposebox} = parts;
              innerComposebox.injectInput(
                  'title', '', FAKE_TOKEN_STRING, /*supportsUnimodal=*/ false,
                  'quoteFilled');
              await innerComposebox.updateComplete;
              await microtasksFinished();

              const file =
                  innerComposebox.attachedContext.get(FAKE_TOKEN_STRING);
              assertTrue(file !== undefined, 'The injected file should exist');
              assertEquals('quoteFilled', file.iconName);

              // Avoid using $.carousel since it may be cached.
              const carousel =
                  innerComposebox.shadowRoot
                      .querySelector<ComposeboxFileCarouselElement>(
                          '#carousel');
              assertTrue(carousel !== null, 'Carousel should be in the DOM');
              assertEquals(1, carousel.files.length);

              innerComposebox.deleteFile(FAKE_TOKEN_STRING);
              await innerComposebox.updateComplete;
              await microtasksFinished();
              assertEquals(
                  null, innerComposebox.shadowRoot.querySelector('#carousel'),
                  'Carousel should be removed from the DOM');
            });

        test(
            'Injected input can be added, then deleted from composebox',
            async () => {
              const {innerComposebox} = parts;
              innerComposebox.injectInput(
                  'title', 'thumbnail.jpg', FAKE_TOKEN_STRING,
                  /*supportsUnimodal=*/ false);
              await innerComposebox.updateComplete;
              await microtasksFinished();

              // Avoid using $.carousel since it may be cached.
              const carousel =
                  innerComposebox.shadowRoot
                      .querySelector<ComposeboxFileCarouselElement>(
                          '#carousel');
              assertTrue(carousel !== null, 'Carousel should be in the DOM');
              assertEquals(1, carousel.files.length);

              await deleteLastFile(innerComposebox);
              await innerComposebox.updateComplete;
              await microtasksFinished();

              assertEquals(
                  null, innerComposebox.shadowRoot.querySelector('#carousel'),
                  'Carousel should be removed from the DOM');
            });
      });
});
