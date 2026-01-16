// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {AutocompleteMatch, AutocompleteResult} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

const ADD_FILE_CONTEXT_FN = 'addFileContext';
const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';
const FAKE_TOKEN_STRING_2 = '00000000000000001234567890ABCDFF';

function pressEnter(element: HTMLElement) {
  element.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Enter',
    bubbles: true,
    composed: true,
  }));
}

function createAutocompleteMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  const base: AutocompleteMatch = {
    allowedToBeDefaultMatch: false,
    isSearchType: true,
    contents: 'a suggestion',
    destinationUrl: {url: `https://google.com/search?q=a+suggestion`},
    fillIntoEdit: 'a suggestion',
    type: 'search-suggest',

    // Add all other fields needed to satisfy the AutocompleteMatch type
    isHidden: false,
    a11yLabel: '',
    actions: [],
    isEnterpriseSearchAggregatorPeopleType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,
    contentsClass: [{offset: 0, style: 0}],
    description: '',
    descriptionClass: [{offset: 0, style: 0}],
    inlineAutocompletion: '',
    iconPath: '',
    iconUrl: {url: ''},
    imageDominantColor: '',
    imageUrl: '',
    isNoncannedAimSuggestion: false,
    removeButtonA11yLabel: '',
    isRichSuggestion: false,
    isWeatherAnswerSuggestion: null,
    answer: null,
    tailSuggestCommonPrefix: null,
    hasInstantKeyword: false,
    keywordChipHint: '',
    keywordChipA11y: '',
  } as AutocompleteMatch;

  return Object.assign(base, modifiers);
}

function createAutocompleteResult(modifiers: Partial<AutocompleteResult> = {}):
    AutocompleteResult {
  const base: AutocompleteResult = {
    input: '',
    matches: [],
    suggestionGroupsMap: {},
    smartComposeInlineHint: null,
  };

  return Object.assign(base, modifiers);
}

function simulateUserInput(inputElement: HTMLInputElement, value: string) {
  inputElement.value = value;
  inputElement.dispatchEvent(
      new Event('input', {bubbles: true, composed: true}));
}


suite('ContextualTasksComposeboxTest', () => {
  let contextualTasksApp: ContextualTasksAppElement;
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
        destinationUrl: {url: `https://google.com/search?q=${testQuery}`},
        type: 'search-what-you-typed',
        fillIntoEdit: testQuery,
      }),
      createAutocompleteMatch(),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
    });

    testProxy = new TestContextualTasksBrowserProxy('https://google.com');
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler as any, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app');

    document.body.appendChild(contextualTasksApp);
    await microtasksFinished();
    composebox = contextualTasksApp.$.composebox.$.composebox;

    assertTrue(
        MockResizeObserver.instances.length >= 1,
        'There should be at least one ResizeObserver instance.');
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  /* Get file from `imageInput` if `fileType` is image;
   * otherwise if pdf, then from files.
   */
  function getInputBasedOnFileType(fileType: string): HTMLInputElement {
    return fileType === 'application/pdf' ? composebox.$.context.$.fileInput :
                                            composebox.$.context.$.imageInput;
  }

  function getMockFileChangeEventForFileType(fileType: string): Event {
    if (fileType === 'application/pdf') {
      return new Event('change');
    }

    const mockFileChange = new Event('change', {bubbles: true});
    // Read only.
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: composebox.$.context.$.imageInput,
    });
    return mockFileChange;
  }

  async function uploadFileAndVerify(
      token: Object, file: File, expectedInitialFilesCount: number = 0) {
    // Assert initial file count if 0 -> carousel should not render.
    if (expectedInitialFilesCount === 0) {
      assertFalse(
          !!composebox.$.context.shadowRoot.querySelector('#carousel'),
          'Files should be empty and carousel should not render.');
    }


    mockSearchboxPageHandler.resetResolver(ADD_FILE_CONTEXT_FN);
    mockSearchboxPageHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve({token: token}));
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);

    const input: HTMLInputElement = getInputBasedOnFileType(file.type);
    input.files = dataTransfer.files;
    input.dispatchEvent(getMockFileChangeEventForFileType(file.type));
    // Must call to upload. Await -> wait for it to be called once.
    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();
    await verifyFileCarouselMatchesUploaded(file, expectedInitialFilesCount);
  }

  async function verifyFileCarouselMatchesUploaded(
      file: File, expectedInitialFilesCount: number) {
    // Assert one file.
    const files = composebox.$.context.$.carousel.files;
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
    const files = composebox.$.context.$.carousel.files;
    const deletedId = files[files.length - 1]!.uuid;
    composebox.$.context.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));
    await microtasksFinished();
  }

  function assertStyle(
      element: Element|null, name: string, expected: string,
      error: string = '') {
    assertTrue(!!element, `Element is null`);
    const actual =
        window.getComputedStyle(element).getPropertyValue(name).trim();
    assertEquals(expected, actual, error);
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
    assertEquals(`https://google.com/search?q=${TEST_QUERY}`, url.url);
    mockTimer.tick(0);

    assertEquals(
        '', inputElement.value,
        'Input should be cleared, but input = ' + inputElement.value);
    assertEquals(
        null, composebox.getMatchesElement().result,
        'Matches should be cleared');
  });

  test('LensButtonTriggersOverlay', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;
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
  });

  test('LensButtonDisabledWhenOverlayShowing', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    contextualTasksApp.$.composebox.isSidePanel = true;
    contextualTasksApp.$.composebox.isLensOverlayShowing = false;
    await microtasksFinished();

    assertFalse(composebox.lensButtonDisabled);

    contextualTasksApp.$.composebox.isLensOverlayShowing = true;
    await microtasksFinished();

    assertTrue(composebox.lensButtonDisabled);
  });

  test('hides composebox and header when hideInput called', async () => {
    const composebox = contextualTasksApp.$.composebox;
    const header = contextualTasksApp.$.composeboxHeaderWrapper;

    testProxy.callbackRouterRemote.hideInput();
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(!!composebox, 'Composebox should exist after hideInput');
    assertTrue(!!header, 'Composebox header should exist after hideInput');

    assertTrue(
        header.hidden, 'Composebox header should be hidden after hideInput');
    assertTrue(
        composebox.hidden, 'Composebox should be hidden after hideInput');

    testProxy.callbackRouterRemote.restoreInput();
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    assertTrue(!!composebox, 'Composebox should exist after restoreInput');
    assertFalse(
        composebox.hidden,
        'Composebox should not be hidden after restoreInput');

    assertTrue(!!header, 'Composebox header should exist after restoreInput');
    assertFalse(
        header.hidden,
        'Composebox header should not be hidden after restoreInput');
  });

  test('Composebox submits then clears input', async () => {
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kProcessing,
        /*error_type=*/ null,
    );
    await microtasksFinished();
    await composebox.$.context.updateComplete;
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
    await microtasksFinished();
    await composebox.$.context.updateComplete;

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertFalse(submitButton?.disabled, 'Submit button should not be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');
    const submitOverlay: HTMLElement|null =
        submitContainer.querySelector('#submitOverlay');

    assertStyle(
        submitButton, 'pointer-events', 'auto',
        'Submit button should not be disabled');
    assertStyle(
        submitContainer, 'cursor', 'pointer',
        'Submit button cursor should be pointer');
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertTrue(!!submitOverlay, 'Submit button overlay should exist');
    submitOverlay?.click();

    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(0, composebox.$.context.files_.size);
  });

  test('Composebox submit button disabled when uploading files', async () => {
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

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
    const submitOverlay: HTMLElement|null =
        submitContainer!.querySelector('#submitOverlay');
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertStyle(
        submitOverlay, 'pointer-events', 'none',
        'Submit button should be disabled');
    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
  });

  test(
      'Upload status is tracked properly when adding and removing files',
      async () => {
        assertEquals(0, composebox.getRemainingFilesToUpload().size);
        const testFile1 = new File(['test'], 'test1.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(FAKE_TOKEN_STRING, testFile1);
        assertEquals(
            1, composebox.getRemainingFilesToUpload().size,
            'First file should be uploading.');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (first file)');
        const testFile2 =
            new File(['test2'], 'test2.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(FAKE_TOKEN_STRING_2, testFile2, 1);

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
            'File should be deleted and number of files left are 1');
      });

  test('Submit button disabled during Upload Failed', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kUploadFailed, null);
    await composebox.$.context.updateComplete;
    await microtasksFinished();

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    const submitOverlay: HTMLElement|null =
        submitContainer!.querySelector('#submitOverlay');
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertStyle(
        submitOverlay, 'pointer-events', 'none',
        'Submit button should be disabled');
    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');

    assertEquals(1, composebox.getRemainingFilesToUpload().size);
  });

  test('Submit button disabled during Validation Failed', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kValidationFailed, null);
    await composebox.$.context.updateComplete;
    await microtasksFinished();

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    const submitOverlay: HTMLElement|null =
        submitContainer!.querySelector('#submitOverlay');
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertStyle(
        submitOverlay, 'pointer-events', 'none',
        'Submit button should be disabled');
    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');

    assertEquals(1, composebox.getRemainingFilesToUpload().size);
  });

  test('Submit button disabled during Processing', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kProcessingSuggestSignalsReady, null);
    await composebox.$.context.updateComplete;
    await microtasksFinished();

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    const submitOverlay: HTMLElement|null =
        submitContainer!.querySelector('#submitOverlay');
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertStyle(
        submitOverlay, 'pointer-events', 'none',
        'Submit button should be disabled');
    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');

    assertEquals(1, composebox.getRemainingFilesToUpload().size);
  });
  test('clear all (cancel) works for uploading set', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, FileUploadStatus.kUploadSuccessful, null);
    await composebox.$.context.updateComplete;
    await microtasksFinished();

    composebox.clearAllInputs(false);
    await microtasksFinished();
    assertEquals(0, composebox.$.context.files_.size);

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    const submitOverlay: HTMLElement|null =
        submitContainer!.querySelector('#submitOverlay');
    assertTrue(!!submitContainer, 'Submit container button should exist');
    assertStyle(
        submitOverlay, 'pointer-events', 'none',
        'Submit button should be disabled');
    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');

    assertEquals(0, composebox.getRemainingFilesToUpload().size);
  });

  test(
      'clear all (cancel) works for uploading set with undeletable files',
      async () => {
        const token1 = FAKE_TOKEN_STRING;
        await uploadFileAndVerify(
            token1, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

        const currentFiles = composebox.$.context.files_;
        currentFiles.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.$.context.requestUpdate();

        await composebox.$.context.updateComplete;
        await microtasksFinished();

        // Now file 1 is not deletable while file 2 is.
        const token2 = FAKE_TOKEN_STRING_2;
        await uploadFileAndVerify(
            token2, new File(['foo2'], 'foo2.jpg', {type: 'image/png'}), 1);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token1, FileUploadStatus.kUploadSuccessful, null);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, FileUploadStatus.kUploadSuccessful, null);
        await composebox.$.context.updateComplete;
        await microtasksFinished();

        // Clear all inputs (only deletes file 2).
        composebox.clearAllInputs(false);
        await composebox.$.context.updateComplete;
        await microtasksFinished();

        assertEquals(1, composebox.getRemainingFilesToUpload().size);

        const submitButton: HTMLButtonElement|null = getSubmitButton();
        const submitContainer: HTMLElement|null = getSubmitContainer();
        const submitOverlay: HTMLElement|null =
            submitContainer!.querySelector('#submitOverlay');
        assertTrue(!!submitButton, 'Submit button should exist');

        // There are no more deletable files, so submit should be disabled.
        assertTrue(submitButton?.disabled, 'Button should be disabled');

        assertTrue(!!submitContainer, 'Submit container button should exist');
        assertStyle(
            submitOverlay, 'pointer-events', 'none',
            'Submit button should be disabled');
        assertStyle(
            submitContainer, 'cursor', 'not-allowed',
            'Submit button cursor should be not-allowed');

        // Reupload 2nd deleted file.
        await uploadFileAndVerify(
            token2, new File(['foo3'], 'foo3.jpg', {type: 'image/png'}), 1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, FileUploadStatus.kUploadSuccessful, null);
        await composebox.$.context.updateComplete;
        await microtasksFinished();

        const currentFiles2 = composebox.$.context.files_;
        currentFiles2.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.$.context.requestUpdate();

        await composebox.$.context.updateComplete;
        await microtasksFinished();

        // Clear all inputs (deletes no files).
        composebox.clearAllInputs(false);
        await composebox.$.context.updateComplete;
        await microtasksFinished();
        assertEquals(2, composebox.getRemainingFilesToUpload().size);

        assertTrue(!!submitButton, 'Submit button should exist');
        // There are no more deletable files, so submit should be disabled.
        assertTrue(submitButton?.disabled, 'Button should be disabled');

        assertTrue(!!submitContainer, 'Submit container button should exist');
        assertStyle(
            submitOverlay, 'pointer-events', 'none',
            'Submit button should be disabled');
        assertStyle(
            submitContainer, 'cursor', 'not-allowed',
            'Submit button cursor should be not-allowed');
        assertEquals(2, composebox.getRemainingFilesToUpload().size);
      });

  test('FocusUpdatesProperty', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const innerComposebox = composeboxElement.$.composebox;

    innerComposebox.dispatchEvent(new CustomEvent('composebox-focus-in'));
    mockTimer.tick(0);  // Attribute reflection is async
    assertTrue(composeboxElement.isComposeboxFocusedForTesting);

    innerComposebox.dispatchEvent(new CustomEvent('composebox-focus-out'));
    assertTrue(!composeboxElement.isComposeboxFocusedForTesting);
  });

  test('ResizeUpdatesHeight', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const innerComposebox = composeboxElement.$.composebox;


    innerComposebox.style.display = 'block';
    innerComposebox.style.height = '100px';


    Object.defineProperty(innerComposebox, 'offsetHeight', {
      writable: true,
      configurable: true,
      value: 100,
    });

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    const height1 = composeboxElement.composeboxHeightForTesting;
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

    const height2 = composeboxElement.composeboxHeightForTesting;
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
        createAutocompleteResult({input: testQuery, matches: matches}));
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

  test('ExpandAnimationState', function() {
    contextualTasksApp.$.composebox.startExpandAnimation();
    assertEquals('expanding', composebox.animationState);
  });
});
