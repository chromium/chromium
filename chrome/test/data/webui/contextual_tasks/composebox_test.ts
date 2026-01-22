// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {AutocompleteMatch, AutocompleteResult} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {assertStyle, installMock} from './test_utils.js';

const ADD_FILE_CONTEXT_FN = 'addFileContext';
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

async function dispatchDragAndDropEvent(dropZone: Element, files: File[]) {
  if (!dropZone) {
    throw new Error(
        'dispatchDragAndDropEvent: #composebox drop zone not rendered.');
  }

  const enterEvent = createDragEvent('dragenter', files);
  dropZone.dispatchEvent(enterEvent);
  await microtasksFinished();

  const overEvent = createDragEvent('dragover', files);
  dropZone.dispatchEvent(overEvent);
  await microtasksFinished();

  const dropEvent = createDragEvent('drop', files);
  dropZone.dispatchEvent(dropEvent);
  await microtasksFinished();
}

// Creates drag event that is compatible across all OS's + w/bots
function createDragEvent(type: string, files: File[]): DragEvent {
  const event = new DragEvent(type, {
    bubbles: true,
    cancelable: true,
    composed: true,
  });

  const mockDataTransfer = {
    files: files,
    types: ['Files', 'text/plain'],
    items: files.map(f => ({
                       kind: 'file',
                       type: f.type,
                       getAsFile: () => f,
                     })),
    effectAllowed: 'all',
    dropEffect: 'copy',
    getData: () => '',
  };

  Object.defineProperty(event, 'dataTransfer', {
    value: mockDataTransfer,
  });
  return event;
}

function createAutocompleteMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  const base: AutocompleteMatch = {
    allowedToBeDefaultMatch: false,
    isSearchType: true,
    contents: 'a suggestion',
    destinationUrl: `https://google.com/search?q=a+suggestion`,
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
    iconUrl: '',
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

class MockSpeechRecognition {
  voiceSearchInProgress: boolean = false;
  onresult:
      ((this: MockSpeechRecognition,
        ev: SpeechRecognitionEvent) => void)|null = null;
  onend: (() => void)|null = null;
  onerror:
      ((this: MockSpeechRecognition,
        ev: SpeechRecognitionErrorEvent) => void)|null = null;
  interimResults = true;
  continuous = false;
  constructor() {
    mockSpeechRecognition = this;
  }
  start() {
    this.voiceSearchInProgress = true;
  }
  stop() {
    this.voiceSearchInProgress = false;
  }
  abort() {
    this.voiceSearchInProgress = false;
    if (this.onend) {
      this.onend();
    }
  }
}

let mockSpeechRecognition: MockSpeechRecognition;

function createResults(n: number): globalThis.SpeechRecognitionEvent {
  return {
    results: Array.from(Array(n)).map(() => {
      return {
        isFinal: false,
        0: {
          transcript: 'foo',
          confidence: 1,
        },
      } as unknown as SpeechRecognitionResult;
    }),
    resultIndex: 0,
  } as unknown as SpeechRecognitionEvent;
}

function getVoiceSearchButton(composeboxElement: ComposeboxElement):
    HTMLElement|null {
  const contextElement = composeboxElement.$.context;
  return contextElement.shadowRoot.querySelector<HTMLElement>(
      '#voiceSearchButton');
}

function getTransitionEndPromise(
    element: HTMLElement, property?: string): Promise<void> {
  return new Promise<void>(
      resolve =>
          element.addEventListener('transitionend', (e: TransitionEvent) => {
            if (!property || e.propertyName === property) {
              resolve();
            }
          }));
}

function simulateUserInput(inputElement: HTMLInputElement, value: string) {
  inputElement.value = value;
  inputElement.dispatchEvent(
      new Event('input', {bubbles: true, composed: true}));
}

const waitForDisplayNone = (voiceSearchElement: HTMLElement) =>
    new Promise<void>(resolve => {
      const check = () => {
        if (getComputedStyle(voiceSearchElement).display === 'none') {
          resolve();
        } else {
          requestAnimationFrame(check);
        }
      };
      check();
    });

suite('ContextualTasksComposeboxTest', () => {
  let contextualTasksApp: MockContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let windowProxy: TestMock<WindowProxy>;
  let mockTimer: MockTimer;
  let metrics: MetricsTracker;

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

    metrics = fakeMetricsPrivate();

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

    contextualTasksApp = document.createElement('contextual-tasks-app') as
        unknown as MockContextualTasksAppElement;
    document.body.appendChild(contextualTasksApp);
    await microtasksFinished();
    composebox = contextualTasksApp.$.composebox.$.composebox;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);
    window.webkitSpeechRecognition =
        MockSpeechRecognition as unknown as typeof SpeechRecognition;

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
    assertEquals(`https://google.com/search?q=${TEST_QUERY}`, url);
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
    await microtasksFinished();

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

    assertStyle(
        submitButton, 'pointer-events', 'auto',
        'Submit button should not be disabled');
    assertStyle(
        submitContainer, 'cursor', 'pointer',
        'Submit button cursor should be pointer');
    assertTrue(!!submitContainer, 'Submit container button should exist');

    // `submitContainer` must be clickable for tabbing->enter to submit to work.
    submitContainer?.click();

    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Query is submitted via submitQuery_()');

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
    assertTrue(!!submitContainer, 'Submit container button should exist');
    const submitOverlay: HTMLElement|null =
        submitContainer.querySelector('#submitOverlay');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when disabled.');

    submitContainer?.click();
    submitButton?.click();
    submitOverlay?.click();

    await composebox.updateComplete;
    await microtasksFinished();

    assertEquals(
        composebox.animationState, GlowAnimationState.EXPANDING,
        'Query is not submitted via submitQuery_()');
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

  test('sets is-dragging-file attribute on dragenter', async () => {
    // Get composebox div in cr-composebox
    const dropZone = composebox.$.composebox;

    assertFalse(composebox.hasAttribute('is-dragging-file'));

    dropZone.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(composebox.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.DRAGGING, composebox.animationState);
  });

  test('removes is-dragging-file attribute on dragleave', async () => {
    const dropZone = composebox.$.composebox;

    composebox.animationState = GlowAnimationState.DRAGGING;
    dropZone.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    dropZone.dispatchEvent(new DragEvent('dragleave', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(composebox.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.NONE, composebox.animationState);
  });

  test('accepts a dropped file and adds it to the carousel', async () => {
    const dropZone = composebox.$.composebox;
    // Same token for auto inject (mac) and manual (linux/windows)
    const sharedToken = '12345678-1234-1234-1234-123456789abc';
    mockSearchboxPageHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve({token: sharedToken}));

    const file = new File(['content'], 'foo.pdf', {type: 'application/pdf'});
    // Automatically add file (Mac)
    await dispatchDragAndDropEvent(dropZone, [file]);

    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
    assertFalse(composebox.hasAttribute('is-dragging-file'));

    const context = composebox.$.context;
    // Mock backend response: manually add file to frontend to render it in the
    // frontend
    const mockAddedFile: ComposeboxFile = {
      uuid: sharedToken,
      name: 'foo.pdf',
      status: 0,
      type: 'application/pdf',
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
    };
    context.onFileContextAdded(mockAddedFile);
    await microtasksFinished();
    await context.updateComplete;
    await microtasksFinished();

    const carousel: ComposeboxFileCarouselElement|null =
        context.shadowRoot.querySelector('cr-composebox-file-carousel');

    assertTrue(!!carousel, 'Carousel should render');

    const carouselFiles = carousel.files;
    assertEquals(1, carouselFiles.length);
    assertEquals('foo.pdf', carouselFiles[0]!.name);
  });

  test('does not accept wrong file type', async () => {
    const dropZone = composebox.$.composebox;
    const testFile =
        new File(['foo'], 'malware.exe', {type: 'application/x-msdownload'});
    await dispatchDragAndDropEvent(dropZone, [testFile]);

    assertEquals(0, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('accepts images', async () => {
    const dropZone = composebox.$.composebox;
    const file = new File(['content'], 'foo.png', {type: 'image/png'});
    await dispatchDragAndDropEvent(dropZone, [file]);
    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('ExpandAnimationState', function() {
    contextualTasksApp.$.composebox.startExpandAnimation();
    assertEquals('expanding', composebox.animationState);
  });

  test('voice search starts as hidden', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const voiceSearchElement = (composebox as any).$.voiceSearch;
    await waitForDisplayNone(voiceSearchElement);
    assertStyle(voiceSearchElement, 'display', 'none');
  });

  test(
      'clicking voice search starts speech recognition, hides the composebox',
      async () => {
        const composeboxDiv =
            contextualTasksApp.$.composebox.$.composebox.$.composebox;
        const composebox = contextualTasksApp.$.composebox.$.composebox;
        const hidePromise = getTransitionEndPromise(composeboxDiv, 'opacity');
        const voiceSearchButton = getVoiceSearchButton(composebox);
        voiceSearchButton!.click();
        await microtasksFinished();
        await hidePromise;

        assertTrue(mockSpeechRecognition.voiceSearchInProgress);
        assertStyle(composeboxDiv, 'opacity', '0');
        assertStyle(composebox.$.voiceSearch, 'display', 'inline');
        assertEquals(composebox.animationState, GlowAnimationState.LISTENING);
        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
            'Voice search button clicked metric count is incorrect');
      });

  test('on voice search result updates the searchbox input', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;

    const voiceSearchButton = getVoiceSearchButton(composebox);
    voiceSearchButton!.click();
    await microtasksFinished();
    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
        'Voice search button clicked metric count is incorrect');

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world'});

    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    const voiceSearchElement = (composebox as any).$.voiceSearch;
    const voiceSearchInput = voiceSearchElement.$.input;

    assertEquals('helloworld', voiceSearchInput.value);
    await microtasksFinished();

    assertEquals(
        0,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_TRANSCRIPTION_SUCCESS */ 2),
        'Voice search transcription success\
                metric count is incorrect for "helloworld"s');

    const result2 = createResults(2);
    Object.assign(result2.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result2.results[1]![0]!, {transcript: 'hellogoodbye'});
    /* Done with transcribing once there is one `isFinal`.
     * This is because it is in continuous mode. Means terminate and
     * take the specific result marked with `resultIndex`.
     */
    Object.assign(result2.results[1]!, {isFinal: true});
    (result2 as any).resultIndex = 1;
    mockSpeechRecognition.onresult!(result2);
    await microtasksFinished();

    assertEquals('hellogoodbye', composebox.$.input.value);
    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_TRANSCRIPTION_SUCCESS */ 1),
        'Voice search transcription success\
                metric count is incorrect for "hellogoodbye"');
  });

  test(
      'voice search with final result submits metric when idle out',
      async () => {
        const composebox = contextualTasksApp.$.composebox.$.composebox;

        const voiceSearchButton = getVoiceSearchButton(composebox);
        voiceSearchButton!.click();
        await microtasksFinished();

        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
            'Voice search button clicked metric count is incorrect');

        const voiceSearchElement = (composebox as any).$.voiceSearch;
        voiceSearchElement.finalResult_ = 'test';
        voiceSearchElement.onIdleTimeout_();
        await microtasksFinished();

        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_TRANSCRIPTION_SUCCESS */ 1),
            'Voice search transcription success\
                metric count is incorrect for idle timeout');
        assertEquals('test', composebox.$.input.value);
      });

  test('on error shows error container for NOT_ALLOWED', async () => {
    const composeboxDiv =
        contextualTasksApp.$.composebox.$.composebox.$.composebox;
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const voiceSearchButton = getVoiceSearchButton(composebox);
    voiceSearchButton!.click();
    await microtasksFinished();

    const hidePromise = getTransitionEndPromise(composeboxDiv, 'opacity');

    mockSpeechRecognition.onerror!
        ({error: 'not-allowed'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();
    await hidePromise;
    await composebox.updateComplete;

    const voiceSearchElement = composebox.$.voiceSearch;
    const errorContainer =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#error-container');
    const inputElement =
        voiceSearchElement.shadowRoot.querySelector<HTMLTextAreaElement>(
            '#input');

    assertTrue(!!errorContainer);
    assertFalse(errorContainer.hidden);
    assertFalse(errorContainer.hidden, 'Error container should not be hidden');
    assertTrue(inputElement!.hidden);
    assertStyle(composeboxDiv, 'opacity', '0');
    assertStyle(composebox.$.voiceSearch, 'display', 'inline');
    assertEquals(composebox.animationState, GlowAnimationState.LISTENING);

    mockSpeechRecognition.onend!();
    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_ERROR */ 2),
        'Voice search error metric count is incorrect');
  });

  test(
      'on voice search error does not show non-NOT-ALLOWED errors, \
      and then hides overlay',
      async () => {
        const composeboxDiv =
            contextualTasksApp.$.composebox.$.composebox.$.composebox;
        const composebox = contextualTasksApp.$.composebox.$.composebox;
        composebox.$.voiceSearch.start();
        await microtasksFinished();
        assertEquals(
            0,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
            'Voice search button clicked metric count is incorrect');

        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await composebox.updateComplete;
        await microtasksFinished();

        const voiceSearchElement = composebox.$.voiceSearch;
        const errorContainer =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#error-container');
        assertTrue(!!errorContainer);
        assertTrue(errorContainer.hidden);

        mockTimer.tick(0);
        await microtasksFinished();
        await waitForDisplayNone(voiceSearchElement);
        assertStyle(voiceSearchElement, 'display', 'none');
        assertStyle(composeboxDiv, 'display', 'flex');

        mockSpeechRecognition.onend!();
        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_ERROR_AND_CANCELED */ 3),
            'Voice search error-canceled metric count is incorrect');
      });

  test('clicking cancel button cancels voice search', async () => {
    const composeboxDiv =
        contextualTasksApp.$.composebox.$.composebox.$.composebox;
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const voiceSearchButton = getVoiceSearchButton(composebox);
    const voiceSearchElement = composebox.$.voiceSearch;
    voiceSearchButton!.click();
    await microtasksFinished();

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world'});
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    const voiceSearchInput = voiceSearchElement.$.input;
    const showPromise = getTransitionEndPromise(composeboxDiv, 'opacity');

    assertEquals('helloworld', voiceSearchInput.value);

    voiceSearchElement.$.closeButton.click();
    await showPromise;

    await waitForDisplayNone(voiceSearchElement);
    await microtasksFinished();

    assertStyle(
        voiceSearchElement, 'display', 'none', 'Voice search should be hidden');
    assertStyle(composeboxDiv, 'display', 'flex', 'Composebox should be shown');
    assertEquals(composebox.$.input.value, '', 'Input should be cleared');

    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_USER_CANCELED*/ 4),
        'Voice search canceled metric count is incorrect');
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
            contextualTasksApp.$.composebox, 'position', 'static',
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
        contextualTasksApp.$.composebox, 'position', 'static',
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

    await microtasksFinished();
    await composebox.$.context.updateComplete;
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

    await microtasksFinished();
    await composebox.$.context.updateComplete;

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

    assertEquals(0, composebox.$.context.files_.size);
  });
});
