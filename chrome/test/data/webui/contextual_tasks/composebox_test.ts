// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {FileUploadStatus, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {assertStyle, mockInputState} from './test_utils.js';

const ADD_FILE_CONTEXT_FN = 'addFileContext';
const ADD_TAB_CONTEXT_FN = 'addTabContext';
const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';
const FAKE_TOKEN_STRING_2 = '00000000000000001234567890ABCDFF';

type MockContextualTasksAppElement =
    Omit<ContextualTasksAppElement,|'isZeroState_'|'isShownInTab_'>&{
      isZeroState_: boolean,
      isShownInTab_: boolean,
    };

function pressEnter(element: HTMLElement) {
  element.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Enter',
    bubbles: true,
    composed: true,
  }));
}

function simulateUserInput(inputElement: HTMLInputElement, value: string) {
  inputElement.value = value;
  inputElement.dispatchEvent(
      new Event('input', {bubbles: true, composed: true}));
}

suite('ContextualTasksComposeboxTest', () => {
  let contextualTasksApp: MockContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let mockTimer: MockTimer;

  async function setupAutocompleteResults(
      searchboxCallbackRouterRemote: SearchboxPageRemote, testQuery: string) {
    const matches = [
      createAutocompleteMatch({
        allowedToBeDefaultMatch: true,
        contents: testQuery,
        destinationUrl: `https://google.com/search?q=${testQuery}`,
        type: 'search-what-you-typed',
        fillIntoEdit: testQuery,
      }),
      createAutocompleteMatch(),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: testQuery,
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    mockTimer.tick(0);
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
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Mock ResizeObserver
    window.ResizeObserver = MockResizeObserver;
    MockResizeObserver.instances = [];

    mockTimer = new MockTimer();

    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
      composeboxHintTextLensOverlay: 'Test Lens Hint',
    });

    testProxy = new TestContextualTasksBrowserProxy('https://google.com');
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));
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
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler as any, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app') as
        unknown as MockContextualTasksAppElement;
    document.body.appendChild(contextualTasksApp);
    await microtasksFinished();
    composebox = contextualTasksApp.$.composebox.$.composebox;

    assertTrue(
        MockResizeObserver.instances.length >= 1,
        'There should be at least one ResizeObserver instance.');

    searchboxCallbackRouterRemote.onInputStateChanged(mockInputState);
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  async function uploadFileAndVerify(
      token: Object, file: File, expectedInitialFilesCount: number = 0) {
    // Assert initial file count if 0 -> carousel should not render.
    if (expectedInitialFilesCount === 0) {
      assertFalse(
          !!composebox.shadowRoot.querySelector('#carousel'),
          'Files should be empty and carousel should not render.');
    }

    mockSearchboxPageHandler.resetResolver(ADD_FILE_CONTEXT_FN);
    mockSearchboxPageHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(token));
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);

    composebox.$.fileInputs.dispatchEvent(
      new CustomEvent('on-file-change', {
          detail: {files: dataTransfer.files},
          bubbles: true,
          composed: true,
        }));

    // Must call to upload. Await -> wait for it to be called once.
    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);

    // Must await for file carousel to re-render since are adding files.
    await composebox.updateComplete;
    await microtasksFinished();
    await verifyFileCarouselMatchesUploaded(file, expectedInitialFilesCount);
  }

  async function verifyFileCarouselMatchesUploaded(
      file: File, expectedInitialFilesCount: number) {
    // Assert one file.

    // Avoid using $.carousel since may be cached.
    const carousel = composebox.shadowRoot.querySelector('#carousel');

    assertTrue(!!carousel, 'Carousel should be in the DOM');
    const files = carousel.files;

    assertEquals(
        expectedInitialFilesCount + 1,
        files.length,
        `Number of carousel files should be ${expectedInitialFilesCount + 1}`,
    );
    const currentFile = files[files.length - 1];

    assertEquals(currentFile!.type, file.type);
    assertEquals(currentFile!.name, file.name);

    // Assert file is uploaded.
    assertEquals(
        1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN),
        `Add file context should be called for this file once.`);
    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    // Verify identity of latest file with that of uploaded version.
    const allCalls = mockSearchboxPageHandler.getArgs(ADD_FILE_CONTEXT_FN);
    const [fileInfo, fileData] = allCalls[allCalls.length - 1];
    assertEquals(fileInfo.fileName, file.name);
    assertDeepEquals(fileData.bytes, fileArray);
  }

  async function deleteLastFile() {
    const files = composebox.$.carousel.files;
    const deletedId = files[files.length - 1]!.uuid;
    composebox.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));
    await microtasksFinished();
  }

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
    contextualTasksApp.setIsZeroStateForTesting(true);
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
    contextualTasksApp.setIsZeroStateForTesting(false);
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

    await setupAutocompleteResults(searchboxCallbackRouterRemote, TEST_QUERY);

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
        contextualTasksApp = document.createElement('contextual-tasks-app') as
            unknown as MockContextualTasksAppElement;
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
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kProcessingSuggestSignalsReady,
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
        FileUploadStatus.kUploadStarted,
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
        FileUploadStatus.kProcessing,
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
        FileUploadStatus.kUploadSuccessful,
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
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kProcessingSuggestSignalsReady,
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
        FAKE_TOKEN_STRING, FileUploadStatus.kUploadReplaced, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
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
        new File(['foo2'], 'foo2.jpg', {type: 'image/jpeg'}),
        /*expectedInitialFilesCount=*/ 0);
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING_2,
        FileUploadStatus.kProcessing,
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
        FileUploadStatus.kUploadReplaced,
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
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kProcessingSuggestSignalsReady,
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
        FAKE_TOKEN_STRING, FileUploadStatus.kUploadSuccessful, null);

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
        FileUploadStatus.kProcessingSuggestSignalsReady,
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
        FileUploadStatus.kUploadSuccessful,
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
        composebox.addFileContextFromBrowser_(FAKE_TOKEN_STRING, fileInfo);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            FileUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        const remaining = composebox.getRemainingFilesToUpload();

        assertEquals(1, remaining.size, 'Pending uploads should increase');
        assertTrue(
            remaining.has(FAKE_TOKEN_STRING),
            'The set should contain our specific UUID');

        assertFalse(
            composebox.fileUploadsComplete,
            'fileUploadsComplete should be false');

        const submitButton: HTMLButtonElement|null = getSubmitButton();

        assertTrue(!!submitButton, 'Submit button should exist');
        assertTrue(submitButton?.disabled, 'Submit button should be disabled');
        // Simulate tab upload success.
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            FileUploadStatus.kUploadSuccessful,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        await microtasksFinished();
        await composebox.updateComplete;
        const submitContainer: HTMLElement|null = getSubmitContainer();
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

        assertEquals(0, composebox.files_.size);

        // Should be no longer `EXPANDING` after successful upload and submit
        // click.
        assertNotEquals(
            composebox.animationState, GlowAnimationState.EXPANDING);
      });

  test(
      'Upload status is tracked properly when adding and removing files',
      async () => {
        assertEquals(0, composebox.getRemainingFilesToUpload().size);
        const testFile1 = new File(['test'], 'test1.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(FAKE_TOKEN_STRING, testFile1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            FileUploadStatus.kNotUploaded,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            0, composebox.getRemainingFilesToUpload().size,
            'First file should be uploading.');
        assertTrue(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (first file)');

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            FileUploadStatus.kProcessing,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            1, composebox.getRemainingFilesToUpload().size,
            'First file should be uploading.');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (first file)');
        const testFile2 =
            new File(['test2'], 'test2.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(FAKE_TOKEN_STRING_2, testFile2, 1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING_2,
            FileUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            2, composebox.getRemainingFilesToUpload().size,
            'Second file should be uploading');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (second file)');

        await deleteLastFile();
        assertEquals(
            1, composebox.getRemainingFilesToUpload().size,
            'File should be deleted and number of files left are 1');

        await deleteLastFile();
        assertEquals(
            0, composebox.getRemainingFilesToUpload().size,
            'File should be deleted and number of files left are 0');
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
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kProcessing, null);

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
        token, FileUploadStatus.kUploadFailed, null);
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
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kProcessing, null);
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
        token, FileUploadStatus.kValidationFailed, null);

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
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kProcessing, null);
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
        token, FileUploadStatus.kUploadExpired, null);
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
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kProcessingSuggestSignalsReady, null);

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

  test('clear all (cancel) works for uploading set', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    await composebox.updateComplete;
    await microtasksFinished();

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kUploadSuccessful, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await microtasksFinished();

    composebox.clearAllInputs(false);

    await Promise.all([
      composebox.updateComplete,
      microtasksFinished(),
    ]);

    assertEquals(0, composebox.files_.size);

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

    assertEquals(0, composebox.getRemainingFilesToUpload().size);
  });

  test(
      'clear all (cancel) works for uploading set with undeletable files',
      async () => {
        const token1 = FAKE_TOKEN_STRING;
        await uploadFileAndVerify(
            token1, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

        const currentFiles = composebox.files_;
        currentFiles.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.requestUpdate();

        await composebox.updateComplete;
        await microtasksFinished();

        // Now file 1 is not deletable while file 2 is.
        const token2 = FAKE_TOKEN_STRING_2;
        await uploadFileAndVerify(
            token2, new File(['foo2'], 'foo2.jpg', {type: 'image/png'}), 1);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token1, FileUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, FileUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();

        await composebox.updateComplete;
        await microtasksFinished();

        // Clear all inputs (only deletes file 2).
        composebox.clearAllInputs(false);
        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(1, composebox.getRemainingFilesToUpload().size);

        const submitButton: HTMLButtonElement|null = getSubmitButton();
        const submitContainer: HTMLElement|null = getSubmitContainer();
        assertTrue(!!submitButton, 'Submit button should exist');

        // There are no more deletable files, so submit should be disabled.
        assertTrue(submitButton?.disabled, 'Button should be disabled');

        assertTrue(!!submitContainer, 'Submit container button should exist');

        assertStyle(
            submitContainer, 'cursor', 'not-allowed',
            'Submit button cursor should be not-allowed');
        assertStyle(
            submitContainer, 'pointer-events', 'auto',
            'Submit container should still have pointer-events on,\
                even when disabled.');

        // Reupload 2nd deleted file.
        await uploadFileAndVerify(
            token2, new File(['foo3'], 'foo3.jpg', {type: 'image/png'}), 1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, FileUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();
        await composebox.updateComplete;
        await microtasksFinished();

        const currentFiles2 = composebox.files_;
        currentFiles2.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.requestUpdate();

        await composebox.updateComplete;
        await microtasksFinished();

        // Clear all inputs (deletes no files).
        composebox.clearAllInputs(false);
        await composebox.updateComplete;
        await microtasksFinished();
        assertEquals(2, composebox.getRemainingFilesToUpload().size);

        assertTrue(!!submitButton, 'Submit button should exist');
        // There are no more deletable files, so submit should be disabled.
        assertTrue(submitButton?.disabled, 'Button should be disabled');

        assertTrue(!!submitContainer, 'Submit container button should exist');

        assertStyle(
            submitContainer, 'cursor', 'not-allowed',
            'Submit button cursor should be not-allowed');
        assertStyle(
            submitContainer, 'pointer-events', 'auto',
            'Submit container should still have pointer-events on,\
                even when disabled.');
        assertEquals(2, composebox.getRemainingFilesToUpload().size);
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
    const inputElement = composebox.$.input;


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

    const matchesEl = composebox.getMatchesElement();
    assertTrue(!!matchesEl.result, 'Matches should be populated');
    assertEquals(2, matchesEl.result.matches.length, 'Should have 2 matches');


    inputElement.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));

    // Wait for Lit updates to propagate
    mockTimer.tick(100);
    await composebox.getMatchesElement().updateComplete;
    await composebox.updateComplete;
    assertEquals(
        0, composebox.getMatchesElement().selectedMatchIndex,
        'Index should be 0');
    assertEquals(
        'match 1', inputElement.value, 'Input value should be match 1');
    assertEquals(
        0, (composebox as any).selectedMatchIndex_, 'Parent index should be 0');
  });

  test('TooltipVisibilityUpdatesOnResize', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = composeboxElement.$.onboardingTooltip;

    // Force show tooltip
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
    });
    composeboxElement.numberOfTimesTooltipShownForTesting = 0;
    composeboxElement.userDismissedTooltipForTesting = false;

    // Simulate active tab chip token presence
    const innerComposebox = composeboxElement.$.composebox;
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    composeboxElement.updateTooltipVisibilityForTesting();
    assertTrue(tooltip.shouldShow);

    // Resize event
    const resizeEvent = new CustomEvent('composebox-resize', {
      detail: {carouselHeight: 50},
      bubbles: true,
      composed: true,
    });
    innerComposebox.dispatchEvent(resizeEvent);

    // Tooltip should still be shown and position updated (implicitly via resize
    // observer or logic)
    assertTrue(tooltip.shouldShow);
  });

  test('TooltipImpressionIncrementsAfterDelay', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = composeboxElement.$.onboardingTooltip;

    // Force show tooltip with delay.
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
      composeboxShowOnboardingTooltipImpressionDelay: 3000,
    });
    composeboxElement.numberOfTimesTooltipShownForTesting = 0;
    composeboxElement.userDismissedTooltipForTesting = false;

    const innerComposebox = composeboxElement.$.composebox;
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    // Trigger update.
    composeboxElement.updateTooltipVisibilityForTesting();
    assertTrue(tooltip.shouldShow);

    // Should not have incremented yet.
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);

    // Tick almost to the end.
    mockTimer.tick(2999);
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);

    // Tick past the delay.
    mockTimer.tick(1);
    assertEquals(1, composeboxElement.numberOfTimesTooltipShownForTesting);
  });

  test('TooltipImpressionTimerResetsOnHide', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = composeboxElement.$.onboardingTooltip;

    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
      composeboxShowOnboardingTooltipImpressionDelay: 3000,
    });
    composeboxElement.numberOfTimesTooltipShownForTesting = 0;
    composeboxElement.userDismissedTooltipForTesting = false;

    const innerComposebox = composeboxElement.$.composebox;
    // Mock existence of chip.
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    // Show tooltip.
    composeboxElement.updateTooltipVisibilityForTesting();
    assertTrue(tooltip.shouldShow);

    // Advance time partially.
    mockTimer.tick(1000);
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);

    // Hide tooltip (e.g. chip disappears).
    innerComposebox.getHasAutomaticActiveTabChipToken = () => false;
    composeboxElement.updateTooltipVisibilityForTesting();
    assertFalse(tooltip.shouldShow);

    // Advance past original deadline.
    mockTimer.tick(5000);
    // Should NOT have incremented because timer was cleared.
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);
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
        contextualTasksApp.isShownInTab_ = false;
        contextualTasksApp.isZeroState_ = false;

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composeboxHeader, 'font-size', '28px',
            'When in side panel non-zero-state, composebox header font-size');
        assertStyle(
            contextualTasksApp.$.composebox.$.composebox, 'min-width', '200px',
            'When in side panel non-zero-state, composebox min-width');

        contextualTasksApp.isZeroState_ = true;
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

        contextualTasksApp.isZeroState_ = false;
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
    contextualTasksApp.isShownInTab_ = true;
    contextualTasksApp.isZeroState_ = false;
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertStyle(
        contextualTasksApp.$.composebox.$.composebox, 'min-width', '0px',
        'When in full tab mode non-zero-state, composebox min-width');
    assertStyle(
        contextualTasksApp.$.composeboxHeader, 'font-size', '32px',
        'When in full tab mode non-zero-state, composebox header font-size');

    contextualTasksApp.isZeroState_ = true;
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

    contextualTasksApp.isZeroState_ = false;
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

  test('Composebox submits by pressing enter, then clears input', async () => {
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    // Other processing state should result in not ready to submit.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kProcessingSuggestSignalsReady,
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
        FileUploadStatus.kUploadSuccessful,
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

  test('EnterKeyAfterSubmitDoesNotAddNewLine', async () => {
    mockTimer.install();
    const TEST_QUERY = 'test query';

    const inputElement = composebox.$.input;
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
    await setupAutocompleteResults(searchboxCallbackRouterRemote, TEST_QUERY);

    // Wait for the matches to be populated in the dropdown.
    while (!composebox.getMatchesElement().result) {
      mockTimer.tick(10);
      await Promise.resolve();
    }

    const submitButton = getSubmitButton();
    assertTrue(!!submitButton);
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
    await contextualTasksApp.updateComplete;
    assertEquals(
        '', inputElement.value, 'Input should be cleared after submit');

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
    const inputElement = innerComposebox.$.input;
    const keydownDiv =
        innerComposebox.shadowRoot.querySelector<HTMLElement>('#composebox');
    assertTrue(!!keydownDiv);

    assertEquals('', inputElement.value);
    mockSearchboxPageHandler.reset();

    // Action: Press Enter on empty input.
    pressEnter(keydownDiv);
    await microtasksFinished();

    // Assert: No newline and no submission.
    assertFalse(inputElement.value.includes('\n'));
    assertEquals(0, mockSearchboxPageHandler.getCallCount('submitQuery'));
  });

  test('Composebox upload disabled when uploading files', async () => {
    composebox.searchboxLayoutMode = '';
    composebox.contextMenuEnabled_ = true;
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    assertFalse(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be enabled');

    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    // Other processing state should result in not ready to submit.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kProcessingSuggestSignalsReady,
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
    assertTrue(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be disabled while uploading');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        0, composebox.getRemainingFilesToUpload().size,
        '0 Files should be uploading');
    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    assertFalse(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be re-enabled after upload');
  });

  test(
      'Composebox upload disabled when uploading files with contextMenu',
      async () => {
        composebox.searchboxLayoutMode = '';
        composebox.contextMenuEnabled_ = true;
        await composebox.updateComplete;
        await microtasksFinished();
        const contextEntrypoint =
            composebox.shadowRoot.querySelector('#contextEntrypoint');
        assertTrue(!!contextEntrypoint);
        const entrypointMenu = contextEntrypoint.$.entrypointMenu;
        assertTrue(!!entrypointMenu, 'Context menu should exist');
        const button = entrypointMenu.shadowRoot?.querySelector('#entrypoint');
        assertTrue(!!button, 'Context menu button should exist');
        assertFalse(!!button.disabled);

        await uploadFileAndVerify(
            FAKE_TOKEN_STRING,
            new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

        // Other processing state should result in not ready to submit.
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            FileUploadStatus.kProcessingSuggestSignalsReady,
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

        assertTrue(!!button.disabled);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            FileUploadStatus.kUploadSuccessful,
            /*error_type=*/ null,
        );

        await microtasksFinished();
        await composebox.updateComplete;

        assertFalse(!!button.disabled);
      });

  test('image upload calls handler for image', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(
        new CustomEvent('open-image-upload', {
          detail: {isImage: true},
          bubbles: true,
          composed: true,
        }));

    await mockComposeboxPageHandler.whenCalled('handleFileUpload');
    assertEquals(1, mockComposeboxPageHandler.getCallCount('handleFileUpload'));
    const [isImage] = mockComposeboxPageHandler.getArgs('handleFileUpload');
    assertTrue(isImage);
  });

  test('file upload calls handler for file', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(
        new CustomEvent('open-file-upload', {
          detail: {isImage: false},
          bubbles: true,
          composed: true,
        }));

    await mockComposeboxPageHandler.whenCalled('handleFileUpload');
    assertEquals(1, mockComposeboxPageHandler.getCallCount('handleFileUpload'));
    const [isImage] = mockComposeboxPageHandler.getArgs('handleFileUpload');
    assertFalse(isImage);
  });

  test('queries autocomplete on load when isZeroState is true', async () => {
    // Clear the body and reset the mock to test a fresh instance.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockSearchboxPageHandler.reset();
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

    const app = document.createElement('contextual-tasks-app') as unknown as
        MockContextualTasksAppElement;
    app.isZeroState_ = false;
    document.body.appendChild(app);
    await app.updateComplete;
    await microtasksFinished();

    // Reset so that way any calls that happen before
    // adding to document do not count (since before that,
    // we are just setting up the test).
    mockSearchboxPageHandler.reset();

    // Mock `isZeroState_` updating value from parent.
    app.isZeroState_ = true;
    await microtasksFinished();

    assertEquals(1, mockSearchboxPageHandler.getCallCount('queryAutocomplete'));
  });

  test(
      'lens button visibility depends on whether DeepSearch is selected in nextbox',
      async () => {
        const contextualComposebox = contextualTasksApp.$.composebox;
        const innerComposebox = contextualComposebox.$.composebox;

        // Ensure we are in side panel mode
        contextualComposebox.isSidePanel = true;
        await contextualComposebox.updateComplete;
        await innerComposebox.updateComplete;
        await microtasksFinished();

        const getLensIcon = () =>
            innerComposebox.shadowRoot.querySelector('#lensIcon');

        assertTrue(
            isVisible(getLensIcon()),
            'Lens button should be visible initially');
        assertTrue(
            innerComposebox.showLensButton,
            'Child showLensButton should be true initially');

        // Enable Deep Search
        innerComposebox.dispatchEvent(
            new CustomEvent('active-tool-mode-changed', {
              bubbles: true,
              composed: true,
              detail: {value: 1},
            }));

        await microtasksFinished();
        await contextualComposebox.updateComplete;
        await innerComposebox.updateComplete;

        // Check the effect
        assertEquals(
            null, getLensIcon(),
            'Lens button should be hidden when Deep Search is active');

        // Disable Deep Search
        innerComposebox.dispatchEvent(
            new CustomEvent('active-tool-mode-changed', {
              bubbles: true,
              composed: true,
              detail: {value: 0},
            }));

        await microtasksFinished();
        await contextualComposebox.updateComplete;
        await innerComposebox.updateComplete;

        // Check the effect
        assertTrue(
            isVisible(getLensIcon()), 'Lens button should be visible again');
      });

  test(
      'does not query autocomplete on load when isZeroState is false',
      async () => {
        // Clear the body and reset the mock to test a fresh instance.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        mockSearchboxPageHandler.reset();
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

        const app = document.createElement('contextual-tasks-app') as unknown as
            MockContextualTasksAppElement;
        app.isZeroState_ = false;
        document.body.appendChild(app);

        await app.updateComplete;
        await microtasksFinished();

        // Reset so that way any calls that happen before
        // adding to document do not count (since before that,
        // we are just setting up the test).
        mockSearchboxPageHandler.reset();

        // Mock `isZeroState_` updating value from parent.
        app.isZeroState_ = false;
        await microtasksFinished();

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
    const inputElement = innerComposebox.$.input;

    // Initially false, placeholder override should be empty.
    assertFalse(contextualComposebox.maybeShowOverlayHintText);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;
    assertEquals('', innerComposebox.inputPlaceholderOverride);

    const initialPlaceholder = inputElement.placeholder;

    // Set to true.
    contextualComposebox.maybeShowOverlayHintText = true;
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertTrue(contextualComposebox.maybeShowOverlayHintText);
    assertEquals('Test Lens Hint', innerComposebox.inputPlaceholderOverride);
    assertEquals('Test Lens Hint', inputElement.placeholder);

    // Set back to false.
    contextualComposebox.maybeShowOverlayHintText = false;
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertFalse(contextualComposebox.maybeShowOverlayHintText);
    assertEquals('', innerComposebox.inputPlaceholderOverride);
    assertEquals(initialPlaceholder, inputElement.placeholder);
  });

  test('SuggestionsHiddenWhenDropdownNotShown', async () => {
    loadTimeData.overrideValues({
      composeboxShowTypedSuggestWithContext: false,
      enableNativeZeroStateSuggestions: true,
    });

    contextualTasksApp.isShownInTab_ = true;
    const contextualComposebox = contextualTasksApp.$.composebox;
    contextualTasksApp.isZeroState_ = true;
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    const suggestionsContainer =
        contextualComposebox.$.contextualTasksSuggestionsContainer;
    assertTrue(!!suggestionsContainer, 'Suggestions container should exist');

    // Initial state: No matches yet, so show-dropdown_ should be false.
    assertFalse(
        composebox.hasAttribute('show-dropdown_'),
        'Dropdown should not be shown initially');
    assertEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be hidden when dropdown is not shown');

    // Add a file.
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    await uploadFileAndVerify(FAKE_TOKEN_STRING, file);

    // Provide ZPS matches (empty query).
    await setupAutocompleteResults(searchboxCallbackRouterRemote, '');
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    // show-dropdown_ should be true now because we have ZPS matches and no
    // input.
    assertTrue(
        composebox.hasAttribute('show-dropdown_'),
        'Dropdown should be shown with ZPS matches after adding a file');

    // The suggestions container should be visible.
    assertNotEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be visible when dropdown is shown');

    // Simulate typing.
    const inputElement = composebox.$.input;
    simulateUserInput(inputElement, 'test');

    // Provide typed matches.
    await setupAutocompleteResults(searchboxCallbackRouterRemote, 'test');
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    // show-dropdown_ should be false because we have a file and
    // composeboxShowTypedSuggestWithContext is false.
    assertFalse(
        composebox.hasAttribute('show-dropdown_'),
        'Dropdown should hide when typing with a file and showTypedSuggestWithContext is false');

    // The CSS rule should hide the suggestions container.
    assertEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be hidden via CSS when dropdown is hidden');
  });

  test('Injected input can be added, then deleted from AIM', async () => {
    composebox.injectInput('title', 'thumbnail.jpg', FAKE_TOKEN_STRING);
    await composebox.$.context.updateComplete;
    await microtasksFinished();

    // Avoid using $.carousel since may be cached.
    const carousel = composebox.$.context.shadowRoot.querySelector('#carousel');
    assertTrue(!!carousel, 'Carousel should be in the DOM');
    const files = carousel.files;
    assertEquals(1, files.length);

    composebox.deleteFile(FAKE_TOKEN_STRING);
    await composebox.$.context.updateComplete;
    await microtasksFinished();
    assertFalse(
        !!composebox.$.context.shadowRoot.querySelector('#carousel'),
        'Carousel should be removed from the DOM');
  });

  test(
      'Injected input can be added, then deleted from composebox', async () => {
        composebox.injectInput('title', 'thumbnail.jpg', FAKE_TOKEN_STRING);
        await composebox.$.context.updateComplete;
        await microtasksFinished();

        // Avoid using $.carousel since may be cached.
        const carousel =
            composebox.$.context.shadowRoot.querySelector('#carousel');
        assertTrue(!!carousel, 'Carousel should be in the DOM');
        const files = carousel.files;
        assertEquals(1, files.length);

        await deleteLastFile();
        await composebox.$.context.updateComplete;
        await microtasksFinished();

        assertFalse(
            !!composebox.$.context.shadowRoot.querySelector('#carousel'),
            'Carousel should be removed from the DOM');
      });
});
