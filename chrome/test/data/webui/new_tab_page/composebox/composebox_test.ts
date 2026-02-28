// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SelectedFileInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ComposeboxElement, ComposeboxProxyImpl, VoiceSearchAction} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {FileUploadErrorType, FileUploadStatus, InputType, ToolMode as ComposeboxToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote, TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, installMock} from '../test_support.js';

enum Attributes {
  SELECTED = 'selected',
}

const ADD_FILE_CONTEXT_FN = 'addFileContext';
const ADD_TAB_CONTEXT_FN = 'addTabContext';
const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';
const FAKE_TOKEN_STRING_2 = '00000000000000001234567890ABCDEE';

const CONTEXT_ADDED_NTP =
    'ContextualSearch.ContextAdded.ContextAddedMethod.NewTabPage';

function generateZeroId(): string {
  // Generate 128 bit unique identifier.
  const components = new Uint32Array(4);
  return components.reduce(
      (id = '', component) => id + component.toString(16).padStart(8, '0'), '');
}

suite('NewTabPageComposeboxTest', () => {
  let composeboxElement: ComposeboxElement;
  let handler: TestMock<PageHandlerRemote>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let metrics: MetricsTracker;

  const deepSearchHint = 'Research anything';
  const imageGenHint = 'Describe your image';
  const canvasHint = 'Create anything';
  const defaultApiHint = loadTimeData.getString('searchboxComposePlaceholder');
  const mockInputState: InputState = {
    hintText: defaultApiHint,
    toolConfigs: [
      {
        tool: ComposeboxToolMode.kDeepSearch,
        hintText: deepSearchHint,
        menuLabel: '',
        chipLabel: '',
        disableActiveModelSelection: false,
        aimUrlParams: [],
      },
      {
        tool: ComposeboxToolMode.kImageGen,
        hintText: imageGenHint,
        menuLabel: '',
        chipLabel: '',
        disableActiveModelSelection: false,
        aimUrlParams: [],
      },
      {
        tool: ComposeboxToolMode.kCanvas,
        hintText: canvasHint,
        menuLabel: '',
        chipLabel: '',
        disableActiveModelSelection: false,
        aimUrlParams: [],
      },
    ],
    modelConfigs: [],
    allowedModels: [],
    allowedTools: [],
    allowedInputTypes: [],
    activeModel: 0,
    activeTool: 0,
    disabledModels: [],
    disabledTools: [],
    disabledInputTypes: [],
    inputTypeConfigs: [],
    toolsSectionConfig: null,
    modelSectionConfig: null,
    maxInstances: {},
    maxTotalInputs: 0,
  };

  setup(() => {
     loadTimeData.overrideValues({
    'composeboxImageFileTypes': 'image/avif,image/bmp,image/jpeg,image/png,image/webp,image/heif,image/heic',
    'composeboxAttachmentFileTypes': '.pdf,application/pdf',
    'contextualMenuUsePecApi': false,
  });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setPromiseResolveFor('getRecentTabs', {tabs: []});
    searchboxHandler.setPromiseResolveFor('getInputState', {
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        inputTypeConfigs: [],
        toolConfigs: [],
        modelConfigs: [],
        toolsSectionConfig: null,
        modelSectionConfig: null,
        hintText: '',
        maxInstances: {},
        maxTotalInputs: 0,
      },
    });
    searchboxCallbackRouterRemote =
        ComposeboxProxyImpl.getInstance()
            .searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
  });

  function createComposeboxElement() {
    composeboxElement = new ComposeboxElement();
    document.body.appendChild(composeboxElement);
  }

  async function waitForAddFileCallCount(expectedCount: number): Promise<void> {
    const startTime = Date.now();
    return new Promise((resolve, reject) => {
      const checkCount = () => {
        const currentCount = searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN);
        if (currentCount === expectedCount) {
          resolve();
          return;
        }

        if (Date.now() - startTime >= 5000) {
          reject(new Error(`Could not add file ${expectedCount} times.`));
          return;
        }

        setTimeout(checkCount, 50);
      };
      checkCount();
    });
  }

  async function addTab() {
    searchboxHandler.setPromiseResolveFor(
        ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING);

    // Assert no files.
    assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

    const contextMenuButton = $$(composeboxElement, '#contextEntrypoint');
    assertTrue(!!contextMenuButton);
    const sampleTabTitle = 'Sample Tab';
    contextMenuButton.dispatchEvent(new CustomEvent('add-tab-context', {
      detail: {id: 1, title: sampleTabTitle},
      bubbles: true,
      composed: true,
    }));

    await searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
    await microtasksFinished();
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'tab');
    assertEquals(files[0]!.name, sampleTabTitle);
    return FAKE_TOKEN_STRING;
  }

  function getInputForFileType(fileType: string): HTMLInputElement {
    return fileType === 'application/pdf' ?
        composeboxElement.$.fileInputs.$.fileInput :
        composeboxElement.$.fileInputs.$.imageInput;
  }

  function getMockFileChangeEventForType(fileType: string): Event {
    if (fileType === 'application/pdf') {
      return new Event('change');
    }

    const mockFileChange = new Event('change', {bubbles: true});
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: composeboxElement.$.fileInputs.$.imageInput,
    });
    return mockFileChange;
  }

  async function areMatchesShowing(): Promise<boolean> {
    // Force a synchronous render.
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    return window.getComputedStyle(composeboxElement.$.matches).display !==
        'none';
  }

  async function uploadFileAndVerify(token: Object, file: File) {
    // Assert no files.
    assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

    searchboxHandler.setPromiseResolveFor(ADD_FILE_CONTEXT_FN, token);

    // Act.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);

    const input: HTMLInputElement = getInputForFileType(file.type);
    input.files = dataTransfer.files;
    input.dispatchEvent(getMockFileChangeEventForType(file.type));

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    assertEquals(searchboxHandler.getCallCount('notifySessionStarted'), 1);
    await verifyFileUpload(file);
  }

  async function verifyFileUpload(file: File) {
    // Assert one file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);

    assertEquals(files[0]!.type, file.type);
    assertEquals(files[0]!.name, file.name);

    // Assert file is uploaded.
    assertEquals(searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 1);

    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    const [[fileInfo, fileData]] = searchboxHandler.getArgs(ADD_FILE_CONTEXT_FN);
    assertEquals(fileInfo.fileName, file.name);
    assertDeepEquals(fileData.bytes, fileArray);
  }
  test(
      'submit disabled when tool is Deep Search (default entrypoint)',
      async () => {
        createComposeboxElement();

        assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

        // Default: submit is disabled with empty input, clicking does nothing.
        composeboxElement.$.submitContainer.click();
        await microtasksFinished();
        assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

        // Change tool to Deep Search
        const inputState = Object.assign({}, mockInputState, {
          activeTool: ComposeboxToolMode.kDeepSearch,
        });
        searchboxCallbackRouterRemote.onInputStateChanged(inputState);
        await searchboxCallbackRouterRemote.$.flushForTesting();

        await microtasksFinished();

        // Submit should still be DISABLED because entrypoint is not
        // ContextualTasks.
        composeboxElement.$.submitContainer.click();
        await microtasksFinished();
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
      });

  test('clear functionality', async () => {
    loadTimeData.overrideValues({composeboxShowSubmit: true});
    createComposeboxElement();
    searchboxHandler.setPromiseResolveFor(
        ADD_FILE_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    // Check submit button disabled.
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'not-allowed');
    // Add input.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
    composeboxElement.$.fileInputs.$.fileInput.files =
        dataTransfer.files;
    composeboxElement.$.fileInputs.$.fileInput.dispatchEvent(
        new Event('change'));

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    /* Submit button will not be enabled since frontend has not been
     * notified that file is done uploading. Carousel should
     * still have the file marked as added.
     */
    assertEquals(composeboxElement.$.carousel.files.length, 1);

    // Clear input.
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await microtasksFinished();

    // Assert
    assertEquals(searchboxHandler.getCallCount('clearFiles'), 1);

    // Check submit button disabled and files empty.
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'not-allowed');
    assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

    // Close composebox.
    const whenCloseComposebox =
        eventToPromise('close-composebox', composeboxElement);
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await whenCloseComposebox;
    assertEquals(searchboxHandler.getCallCount('clearFiles'), 2);
  });

  test('upload image', async () => {
    createComposeboxElement();
    // Submit button is disabled without any input.
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'not-allowed');
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kUploadSuccessful,
        null,
    );
    await composeboxElement.updateComplete;
    await microtasksFinished();

    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'pointer');
  });

  test('upload image works when config is set to wildcard image/*', async () => {
    loadTimeData.overrideValues({
      'composeboxImageFileTypes': 'image/*',
    });
    createComposeboxElement();
    const token = {low: BigInt(1), high: BigInt(2)};
    const file = new File(['foo'], 'foo.jpg', {type: 'image/jpeg'});
    await uploadFileAndVerify(token, file);
  });

  test('uploading/deleting pdf file queries zps', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowSubmit: true});
    createComposeboxElement();
    await microtasksFinished();

    // Autocomplete queried once when composebox is opened.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
    const id = generateZeroId();
    await uploadFileAndVerify(
        id, new File(['foo'], 'foo.pdf', {type: 'application/pdf'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, FileUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    // Autocomplete should be stopped (with matches cleared) and then
    // queried again when a file is uploaded.
    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 1);
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);

    // The suggest request should be triggered before the file has finished
    // uploading.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, FileUploadStatus.kUploadSuccessful, null);

    // Delete the uploaded file.
    const deletedId = composeboxElement.$.carousel.files[0]!.uuid;
    composeboxElement.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();

    // Deleting a file should also stop autocomplete (and clear matches) and
    // then query autocomplete again for unimodal zps results.
    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 2);
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 3);
  });

  test('uploading image file without flag does nothing', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowImageSuggest: false});
    createComposeboxElement();
    await microtasksFinished();

    // Autocomplete queried once when composebox is opened.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
    const id = generateZeroId();
    await uploadFileAndVerify(
        id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, FileUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    // Autocomplete should not be queried again since the uploaded file is an
    // image and the image suggest flag is disabled.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
  });

  test('uploading image file with image suggest flag queries zps', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowImageSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    // Autocomplete queried once when composebox is opened.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
    const id = generateZeroId();
    await uploadFileAndVerify(
        id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, FileUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    // Autocomplete should be stopped (with matches cleared) and then
    // queried again when a file is uploaded.
    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 1);
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);
  });

  [new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
   new File(['foo'], 'foo.pdf', {type: 'application/pdf'})]
      .forEach((file) => {
        test(
            `announce file upload started and completed: ${file.type}`,
            async () => {
              createComposeboxElement();

              let announcementCount = 0;
              const updateAnnouncementCount = () => {
                announcementCount += 1;
              };
              document.body.addEventListener(
                  'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
              let announcementPromise = eventToPromise(
                  'cr-a11y-announcer-messages-sent', document.body);

              const id = generateZeroId();
              await uploadFileAndVerify(id, file);

              let announcement = await announcementPromise;
              assertEquals(announcementCount, 1);
              assertTrue(!!announcement);
              assertEquals(announcement.detail.messages.length, 1);

              searchboxCallbackRouterRemote.onContextualInputStatusChanged(
                  id, FileUploadStatus.kUploadSuccessful, null);
              await searchboxCallbackRouterRemote.$.flushForTesting();

              announcementPromise = eventToPromise(
                  'cr-a11y-announcer-messages-sent', document.body);
              announcement = await announcementPromise;
              assertEquals(announcementCount, 2);
              assertTrue(!!announcement);
              assertEquals(announcement.detail.messages.length, 1);

              // Cleanup event listener.
              document.body.removeEventListener(
                  'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
              assertEquals(
                  1,
                  metrics.count(
                      'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage',
                      0));
            });
      });

  test('upload empty file fails', async () => {
    createComposeboxElement();
    const file = new File([''], 'foo.jpg', {type: 'image/jpeg'});

    // Act.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    const input: HTMLInputElement = getInputForFileType(file.type);
    input.files = dataTransfer.files;
    input.dispatchEvent(getMockFileChangeEventForType(file.type));
    await microtasksFinished();

    // Assert no files uploaded or rendered on the carousel
    assertEquals(searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 0);
    assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));
    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 2));
  });

  test('upload large file fails', async () => {
    const sampleFileMaxSize = 10;
    loadTimeData.overrideValues({'composeboxFileMaxSize': sampleFileMaxSize});
    createComposeboxElement();
    const blob = new Blob(
        [new Uint8Array(sampleFileMaxSize + 1)],
        {type: 'application/octet-stream'});
    const file = new File([blob], 'foo.jpg', {type: 'image/jpeg'});

    // Act.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    const input: HTMLInputElement = getInputForFileType(file.type);
    input.files = dataTransfer.files;
    input.dispatchEvent(getMockFileChangeEventForType(file.type));
    await microtasksFinished();

    // Assert no files uploaded or rendered on the carousel
    assertEquals(searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 0);
    assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));
    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 3));
  });

  [[
    FileUploadStatus.kValidationFailed,
    FileUploadErrorType.kImageProcessingError,
  ],
   [
     FileUploadStatus.kUploadFailed,
     null,
   ],
   [
     FileUploadStatus.kUploadExpired,
     null,
   ],
  ].forEach(([fileUploadStatus, fileUploadErrorType, ..._]) => {
    test(
        `Image upload is removed on failed upload status ${fileUploadStatus}`,
        async () => {
          createComposeboxElement();
          const id = generateZeroId();
          const file = new File(['foo'], 'foo.jpg', {type: 'image/jpeg'});
          await uploadFileAndVerify(id, file);

          searchboxCallbackRouterRemote.onContextualInputStatusChanged(
              id, fileUploadStatus as FileUploadStatus,
              fileUploadErrorType as FileUploadErrorType | null);
          await searchboxCallbackRouterRemote.$.flushForTesting();

          // Assert no files in the carousel.
          assertFalse(
              !!$$<HTMLElement>(composeboxElement, '#carousel'));

          if (fileUploadErrorType !== null) {
            assertEquals(
                loadTimeData.getString('composeFileTypesAllowedError'),
                composeboxElement.$.errorScrim.errorMessage);
          }
        });
  });

  test('upload pdf', async () => {
    createComposeboxElement();
    searchboxHandler.setPromiseResolveFor(
        ADD_FILE_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    // Assert no files.
    assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

    // Arrange.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    composeboxElement.$.fileInputs.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInputs.$.fileInput.dispatchEvent(
        new Event('change'));

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert one pdf file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'application/pdf');
    assertEquals(files[0]!.name, 'foo.pdf');
    assertFalse(!!files[0]!.objectUrl);

    assertEquals(searchboxHandler.getCallCount('notifySessionStarted'), 1);

    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    // Assert file is uploaded.
    assertEquals(searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 1);
    const [[fileInfo, fileData]] =
        searchboxHandler.getArgs(ADD_FILE_CONTEXT_FN);
    assertEquals(fileInfo.fileName, 'foo.pdf');
    assertDeepEquals(fileData.bytes, fileArray);
    // Assert context added method was context menu.
    assertEquals(1, metrics.count(CONTEXT_ADDED_NTP));
    assertEquals(
        1,
        metrics.count(
            CONTEXT_ADDED_NTP,
            /* CONTEXT_MENU */ 0));
  });

  test('delete file', async () => {
    loadTimeData.overrideValues({composeboxFileMaxCount: 5});
    createComposeboxElement();
    let i = 0;
    searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
      i += 1;
      return Promise.resolve({low: BigInt(i + 1), high: BigInt(i + 2)});
    });

    // Arrange.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo'], 'foo.pdf', {type: 'application/pdf'}));
    dataTransfer.items.add(
        new File(['foo2'], 'foo2.pdf', {type: 'application/pdf'}));

    // Since the `onFileChange_` method checks the event target when creating
    // the `objectUrl`, we have to mock it here.
    const mockFileChange = new Event('change', {bubbles: true});
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: composeboxElement.$.fileInputs.$.fileInput,
    });

    composeboxElement.$.fileInputs.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInputs.$.fileInput.dispatchEvent(mockFileChange);

    await waitForAddFileCallCount(2);
    await composeboxElement.updateComplete;
    await microtasksFinished();

    // Assert two files are present initially.
    assertEquals(composeboxElement.$.carousel.files.length, 2);

    // Act.
    const deletedId = composeboxElement.$.carousel.files[0]!.uuid;
    composeboxElement.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();

    // Assert.
    assertEquals(composeboxElement.$.carousel.files.length, 1);
    assertEquals(searchboxHandler.getCallCount('deleteContext'), 1);
    const [idArg, fromChip] = searchboxHandler.getArgs('deleteContext')[0];
    assertEquals(idArg, deletedId);
    assertFalse(fromChip);
  });

  test('NotifySessionStarted called on composebox created', () => {
    // Assert call has not occurred.
    assertEquals(searchboxHandler.getCallCount('notifySessionStarted'), 0);

    createComposeboxElement();

    // Assert call occurs.
    assertEquals(searchboxHandler.getCallCount('notifySessionStarted'), 1);
  });

  test('lens icon click calls handler', async () => {
    createComposeboxElement();

    const lensIcon = $$<HTMLElement>(composeboxElement, '#lensIcon');
    assertTrue(!!lensIcon);

    lensIcon.click();
    await handler.whenCalled('handleFileUpload');
    assertEquals(1, handler.getCallCount('handleFileUpload'));
    const [isImage] = handler.getArgs('handleFileUpload');
    assertTrue(isImage);
  });

  test('lens icon mousedown prevents default', async () => {
    createComposeboxElement();
    await microtasksFinished();

    const lensIcon = $$<HTMLElement>(composeboxElement, '#lensIcon');
    assertTrue(!!lensIcon);

    const event = new MouseEvent(
        'mousedown', {bubbles: true, cancelable: true, composed: true});
    lensIcon.dispatchEvent(event);
    await microtasksFinished();

    assertTrue(event.defaultPrevented);
  });

  test('set and delete visual selection thumbnail', async () => {
    createComposeboxElement();
    await microtasksFinished();

    // Initially, carousel is not shown.
    assertFalse(composeboxElement.hasAttribute('show-file-carousel_'));

    // Set a thumbnail.
    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    searchboxCallbackRouterRemote.addFileContext(FAKE_TOKEN_STRING, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: true,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await microtasksFinished();

    // Assert thumbnail is shown.
    assertTrue(composeboxElement.hasAttribute('show-file-carousel_'));
    const fileCarousel = composeboxElement.$.carousel;
    assertTrue(!!fileCarousel);
    await microtasksFinished();

    assertEquals(fileCarousel.files.length, 1);
    assertDeepEquals(fileCarousel.files[0]!.uuid, FAKE_TOKEN_STRING);
    assertEquals(fileCarousel.files[0]!.dataUrl, thumbnailUrl);
    assertTrue(fileCarousel.files[0]!.isDeletable);

    // Delete the thumbnail.
    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);

    const removeImgButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    assertTrue(!!removeImgButton);
    removeImgButton.click();
    await microtasksFinished();

    // Assert thumbnail is removed.
    assertEquals(searchboxHandler.getCallCount('deleteContext'), 1);
    const [idArg, fromChip] = searchboxHandler.getArgs('deleteContext')[0];
    assertEquals(idArg, FAKE_TOKEN_STRING);
    assertFalse(fromChip);
    // The carousel is removed from the DOM when there are no files, so
    // assert its absence.
    assertFalse(!!composeboxElement.shadowRoot.querySelector('#carousel'));
    assertFalse(composeboxElement.hasAttribute('show-file-carousel_'));
  });

  test('setVisualSelectionThumbnail not deletable', async () => {
    createComposeboxElement();
    await microtasksFinished();

    // Set a thumbnail that is not deletable.
    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    searchboxCallbackRouterRemote.addFileContext(FAKE_TOKEN_STRING, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: false,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await microtasksFinished();

    // Assert thumbnail is shown.
    assertTrue(composeboxElement.hasAttribute('show-file-carousel_'));
    const fileCarousel = composeboxElement.$.carousel;
    assertTrue(!!fileCarousel);
    assertEquals(fileCarousel.files.length, 1);
    assertFalse(fileCarousel.files[0]!.isDeletable);

    // Assert delete button is not present.
    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);
    const removeButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    assertEquals(null, removeButton);
  });

  test('image upload button clicks file input', () => {
    loadTimeData.overrideValues({
      'composeboxShowContextMenu': true,
    });
    createComposeboxElement();
    let clickCalled = false;
    composeboxElement.$.fileInputs.$.imageInput.click = () => {
      clickCalled = true;
    };
    const contextEntrypoint = $$(composeboxElement, '#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(
        new CustomEvent('open-image-upload', {bubbles: true, composed: true}));

    // Assert.
    assertTrue(clickCalled);
  });

  test('file upload button clicks file input', () => {
    loadTimeData.overrideValues({
      'composeboxShowContextMenu': true,
    });
    createComposeboxElement();
    let clickCalled = false;
    composeboxElement.$.fileInputs.$.fileInput.click = () => {
      clickCalled = true;
    };
    const contextEntrypoint = $$(composeboxElement, '#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(
        new CustomEvent('open-file-upload', {bubbles: true, composed: true}));

    // Assert.
    assertTrue(clickCalled);
  });

  test(
      'upload button should not be disabled except when upload is in progress',
      async () => {
        loadTimeData.overrideValues({
          'composeboxShowCreateImageButton': true,
        });
        const testInputState = {
          ...mockInputState,
          maxInstances: {
            [InputType.kBrowserTab]: 1,
            [InputType.kLensImage]: 1,
            [InputType.kLensFile]: 1,
          },
          maxTotalInputs: 1,
        };
        createComposeboxElement();
        searchboxCallbackRouterRemote.onInputStateChanged(testInputState);
        await microtasksFinished();

        searchboxHandler.setPromiseResolveFor(
            ADD_FILE_CONTEXT_FN,
            {token: {low: BigInt(1), high: BigInt(2)}});

        // Upload a PDF file.
        const pdfFile = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(pdfFile);
        composeboxElement.$.fileInputs.$.fileInput.files = dataTransfer.files;
        composeboxElement.$.fileInputs.$.fileInput.dispatchEvent(
            new Event('change'));

        await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
        await microtasksFinished();
        assertFalse(composeboxElement['uploadButtonDisabled_']);

        // Delete the file. `uploadButtonDisabled` should be false.
        const deletedId = composeboxElement.$.carousel.files[0]!.uuid;
        composeboxElement.$.carousel.dispatchEvent(new CustomEvent(
            'delete-file',
            {detail: {uuid: deletedId}, bubbles: true, composed: true}));
        await microtasksFinished();
        assertFalse(composeboxElement['uploadButtonDisabled_']);
        searchboxHandler.resetResolver(ADD_FILE_CONTEXT_FN);
        searchboxHandler.setPromiseResolveFor(
            ADD_FILE_CONTEXT_FN,
            {token: {low: BigInt(3), high: BigInt(4)}});

        // Upload an image file. `uploadButtonDisabled` should be false.
        const imageFile = new File(['foo'], 'foo.png', {type: 'image/png'});
        const dataTransfer2 = new DataTransfer();
        dataTransfer2.items.add(imageFile);

        const imageInput =
            composeboxElement.$.fileInputs.$.imageInput;
        imageInput.files = dataTransfer2.files;
        imageInput.dispatchEvent(new Event('change'));

        await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
        await microtasksFinished();
        assertFalse(composeboxElement['uploadButtonDisabled_']);

        // Enter create image mode.
        composeboxElement['activeToolMode_'] = ComposeboxToolMode.kImageGen;
        await composeboxElement.updateComplete;
        assertFalse(composeboxElement['uploadButtonDisabled_']);

        // Exit create image mode. `uploadButtonDisabled` should be false.
        composeboxElement['activeToolMode_'] = ComposeboxToolMode.kUnspecified;
        await composeboxElement.updateComplete;
        assertFalse(composeboxElement['uploadButtonDisabled_']);
      });

  test('session abandoned on esc click', async () => {
    // Arrange.
    loadTimeData.overrideValues({composeboxCloseByEscape: true});
    createComposeboxElement();

    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const whenCloseComposebox =
        eventToPromise('close-composebox', composeboxElement);

    // Assert call occurs.
    composeboxElement.$.composebox.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Escape'}));
    await microtasksFinished();
    const event = await whenCloseComposebox;
    assertEquals(event.detail.composeboxText, 'test');
    assertEquals(searchboxHandler.getCallCount('clearFiles'), 1);
  });

  test('escape key behavior with suggestions', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({matches}));
    await microtasksFinished();
    assertTrue(await areMatchesShowing());

    // Case 1: composeboxCloseByEscape_ = false. Escape should clear the text.
    (composeboxElement as any).composeboxCloseByEscape_ = false;
    const closePromise = eventToPromise('close-composebox', composeboxElement);
    let closed = false;
    closePromise.then(() => closed = true);

    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    composeboxElement.$.input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, composed: true}));
    await microtasksFinished();

    assertEquals(searchboxHandler.getCallCount('clearFiles'), 1);
    assertFalse(closed);
    assertEquals('', composeboxElement.$.input.value);

    // Case 2: composeboxCloseByEscape_ = true. Escape should close the
    // composebox.
    (composeboxElement as any).composeboxCloseByEscape_ = true;
    const whenCloseComposebox =
        eventToPromise('close-composebox', composeboxElement);
    composeboxElement.$.input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, composed: true}));
    await whenCloseComposebox;
    assertEquals(searchboxHandler.getCallCount('clearFiles'), 2);
  });

  test('session abandoned on cancel button click', async () => {
    // Arrange.
    createComposeboxElement();

    await microtasksFinished();

    // Close composebox.
    const whenCloseComposebox =
        eventToPromise('close-composebox', composeboxElement);
    const cancelIcon = $$<HTMLElement>(composeboxElement, '#cancelIcon');
    assertTrue(!!cancelIcon);
    cancelIcon.click();
    const event = await whenCloseComposebox;
    assertEquals(event.detail.composeboxText, '');
    assertEquals(searchboxHandler.getCallCount('clearFiles'), 1);
  });

  test('submit button click leads to handler called', async () => {
    createComposeboxElement();
    // Assert.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

    // Arrange.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const matches =
        [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test',
          matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    composeboxElement.$.submitContainer.click();
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 1);
  });

  test('submit button is a no-op when disabled', async () => {
    createComposeboxElement();
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

    // Arrange.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    // Assert submit is disabled.
    const submitButton =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#submitIcon');
    assertTrue(!!submitButton);
    assertTrue(submitButton.hasAttribute('disabled'));

    // Act.
    composeboxElement.$.submitContainer.click();
    await microtasksFinished();

    // Assert no calls were made.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);
  });

  test('empty input has disabled submit button', async () => {
    createComposeboxElement();

    // Arrange.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    // Assert call cannot occur.
    const submitButton =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#submitIcon');
    assertTrue(!!submitButton);
    assertTrue(submitButton.hasAttribute('disabled'));
  });

  test('submit button is disabled', async () => {
    // Arrange.
    composeboxElement.$.input.value = ' ';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    // Assert.
    const submitButton =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#submitIcon');
    assertTrue(!!submitButton);
    assertTrue(submitButton.hasAttribute('disabled'));
  });

  test('keydown submit only works for enter', async () => {
    createComposeboxElement();
    // Assert.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

    // Arrange.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const matches =
        [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test',
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    const shiftEnterEvent = new KeyboardEvent('keydown', {
      key: 'Enter',
      shiftKey: true,
      bubbles: true,
      cancelable: true,
    });
    composeboxElement.$.input.dispatchEvent(shiftEnterEvent);
    await microtasksFinished();

    // Assert.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

    const enterEvent = new KeyboardEvent('keydown', {
      key: 'Enter',
      bubbles: true,
      cancelable: true,
    });
    composeboxElement.$.input.dispatchEvent(enterEvent);
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 1);
  });

  test('clear button title changes with input', async () => {
    createComposeboxElement();
    assertEquals(
        composeboxElement.$.cancelIcon.getAttribute('title'),
        loadTimeData.getString('composeboxCancelButtonTitle'));
    // Arrange.
    composeboxElement.$.input.value = 'Test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    // Assert.
    assertEquals(
        composeboxElement.$.cancelIcon.getAttribute('title'),
        loadTimeData.getString('composeboxCancelButtonTitleInput'));
  });

  test('composebox queries autocomplete on load', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    // Autocomplete should be queried when the composebox is created.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);

    // Restore.
    loadTimeData.overrideValues({composeboxShowZps: false});
  });

  test('dropdown shows when suggestions enabled', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add zps input.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
    assertTrue(!!composeboxDropdown);

    // Composebox dropdown should not show for no matches.
    assertTrue(composeboxDropdown.hidden);

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown.hidden);
  });

  test('dropdown does not show for multiline input', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add typed input.
    composeboxElement.$.input.value = 'Test';
    composeboxElement.$.input.style.height = '64px';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
    assertTrue(!!composeboxDropdown);

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are not available.
    assertTrue(composeboxDropdown.hidden);

    // Arrow down should do default action.
    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });

    composeboxElement.$.input.dispatchEvent(arrowDownEvent);
    await microtasksFinished();
    assertFalse(arrowDownEvent.defaultPrevented);
  });

  test('dropdown does not show with multiple context files', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add zps input.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
    assertTrue(!!composeboxDropdown);

    // Add matches and verify dropdown shows.
    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();
    assertFalse(composeboxDropdown.hidden);

    // If multiple context files are added, the dropdown should hide.
    composeboxElement.addFileContextForTesting({
      uuid: FAKE_TOKEN_STRING,
      name: 'foo.jpg',
      status: 0,
      type: 'image/jpeg',
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
    });
    composeboxElement.addFileContextForTesting({
      uuid: FAKE_TOKEN_STRING + '2',
      name: 'foo2.jpg',
      status: 0,
      type: 'image/jpeg',
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
    });
    await microtasksFinished();
    assertTrue(composeboxDropdown.hidden);
  });

  test('arrow keys work for typed suggest', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add typed input.
    composeboxElement.$.input.value = 'Test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
    assertTrue(!!composeboxDropdown);

    const matches = [
      createSearchMatchForTesting(
          {fillIntoEdit: 'hello world 1', allowedToBeDefaultMatch: true}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 3'}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 4'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
          input: 'Test',
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown.hidden);

    const matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(4, matchEls.length);
    const matchEl = matchEls[0];
    assertTrue(!!matchEl);
    // Verbatim match does not show for typed suggest.
    assertStyle(matchEl, 'display', 'none');

    // Arrow down should do default action.
    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });

    composeboxElement.$.input.dispatchEvent(arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);

    // First SHOWN match (second match) is selected.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world 2', composeboxElement.$.input.value);

    // Arrow down should do default action.
    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });

    composeboxElement.$.input.dispatchEvent(arrowUpEvent);
    await microtasksFinished();
    assertTrue(arrowUpEvent.defaultPrevented);
    // Last match gets selected when arrowing up from the first
    // shown match.
    assertTrue(matchEls[3]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world 4', composeboxElement.$.input.value);

    // When arrowing up from last match, first SHOWN match should be selected.
    composeboxElement.$.input.dispatchEvent(arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world 2', composeboxElement.$.input.value);
  });

  test('dropdown does not show when no typed suggestions enabled', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: false});
    createComposeboxElement();
    await microtasksFinished();

    // Add zps input.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
    assertTrue(!!composeboxDropdown);

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown.hidden);

    composeboxElement.$.input.value = 'Hello';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    // Dropdown should not show for typed input when typed suggest is
    // disabled.
    assertTrue(composeboxDropdown.hidden);
  });

  test('dropdown does not show for typed suggest with context', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add typed input.
    composeboxElement.$.input.value = 'Test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
    assertTrue(!!composeboxDropdown);

    const matches = [
      createSearchMatchForTesting(
          {fillIntoEdit: 'hello world 1', allowedToBeDefaultMatch: true}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 3'}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 4'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
          input: 'Test',
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown.hidden);

    // If context files are added, the dropdown should no longer be visible.
    composeboxElement.addFileContextForTesting({
      uuid: FAKE_TOKEN_STRING,
      name: 'foo.jpg',
      status: 0,
      type: 'image/jpeg',
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
    });
    await microtasksFinished();
    assertTrue(composeboxDropdown.hidden);
  });

  test('dropdown does not show for typed suggest with verbatim match only',
       async () => {
        loadTimeData.overrideValues(
            {composeboxShowZps: true, composeboxShowTypedSuggest: true});
        createComposeboxElement();
        await microtasksFinished();

        // Add typed input.
        composeboxElement.$.input.value = 'Test';
        composeboxElement.$.input.dispatchEvent(new Event('input'));
        await microtasksFinished();

        const composeboxDropdown =
            composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
        assertTrue(!!composeboxDropdown);

        const matches = [
          createSearchMatchForTesting(),
        ];
        searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              matches: matches,
              input: 'Test',
            }));
        await microtasksFinished();

        // Dropdown should not show when only the verbatim match is present.
        assertTrue(composeboxDropdown.hidden);
      });

  test('notify browser when image is added in create image mode', async () => {
    loadTimeData.overrideValues({
      composeboxShowZps: true,
      composeboxShowTypedSuggest: false,
      'composeboxFileMaxCount': 1,
    });
    createComposeboxElement();
    await microtasksFinished();

    // Enter create image mode.
    const contextEntrypoint = $$(composeboxElement, '#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(
        new CustomEvent('tool-click', {
          detail: {toolMode: ComposeboxToolMode.kImageGen},
        }));
    await microtasksFinished();
    assertEquals(searchboxHandler.getCallCount('setActiveToolMode'), 1);
    assertEquals(
        ComposeboxToolMode.kImageGen,
        searchboxHandler.getArgs('setActiveToolMode')[0]);

    // Upload an image file. `uploadButtonDisabled` should be false.
    const id = generateZeroId();
    await uploadFileAndVerify(
        id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, FileUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    assertEquals(searchboxHandler.getCallCount('setActiveToolMode'), 2);
    assertEquals(
        ComposeboxToolMode.kImageGen,
        searchboxHandler.getArgs('setActiveToolMode')[0]);

    // Deleting the image should call setCreateImageMode again but with
    // imagePresent false.
    const deletedId = composeboxElement.$.carousel.files[0]!.uuid;
    composeboxElement.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();
    assertEquals(searchboxHandler.getCallCount('setActiveToolMode'), 3);
    assertEquals(
        ComposeboxToolMode.kImageGen,
        searchboxHandler.getArgs('setActiveToolMode')[0]);
  });

  test('arrow up/down moves selection / focus', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add zps input.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));

    assertTrue(await areMatchesShowing());

    const matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });

    composeboxElement.$.input.dispatchEvent(arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);

    // First match is selected
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', composeboxElement.$.input.value);

    // Move the focus to the second match.
    matchEls[1]!.focus();
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));
    await microtasksFinished();

    // Second match is selected and has focus.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world 2', composeboxElement.$.input.value);
    assertEquals(
        matchEls[1], composeboxElement.$.matches.shadowRoot.activeElement);

    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });

    matchEls[1]!.dispatchEvent(arrowUpEvent);
    await microtasksFinished();
    assertTrue(arrowUpEvent.defaultPrevented);

    // First match gets selected and gets focus while focus is in the matches.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', composeboxElement.$.input.value);
    assertEquals(
        matchEls[0], composeboxElement.$.matches.shadowRoot.activeElement);

    // Restore.
    loadTimeData.overrideValues({composeboxShowZps: false});
  });

  test(
      'arrow up/down enables submit for suggestion with no query', async () => {
        loadTimeData.overrideValues({composeboxShowZps: true});
        createComposeboxElement();
        await microtasksFinished();

        // Add zps input.
        composeboxElement.$.input.value = '';
        composeboxElement.$.input.dispatchEvent(new Event('input'));

        const matches = [
          createSearchMatchForTesting({fillIntoEdit: ''}),
        ];
        searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              matches: matches,
            }));

        assertTrue(await areMatchesShowing());

        const matchEls =
            composeboxElement.$.matches.shadowRoot.querySelectorAll(
                'cr-composebox-match');
        assertEquals(1, matchEls.length);

        const arrowDownEvent = new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,  // So it propagates across shadow DOM boundary.
          key: 'ArrowDown',
        });

        composeboxElement.$.input.dispatchEvent(arrowDownEvent);
        await microtasksFinished();
        assertTrue(arrowDownEvent.defaultPrevented);

        // First match is selected
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        assertEquals('', composeboxElement.$.input.value);

        // Assert submit is enabled.
        const submitButton =
            composeboxElement.shadowRoot.querySelector<HTMLElement>(
                '#submitIcon');
        assertTrue(!!submitButton);
        assertFalse(submitButton.hasAttribute('disabled'));

        // By pressing 'Enter' on the button.
        const keydownEvent = (new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,
          key: 'Enter',
        }));
        matchEls[0]!.dispatchEvent(keydownEvent);
        assertTrue(keydownEvent.defaultPrevented);

        await microtasksFinished();

        // Assert call occurs.
        assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 1);

        // Restore.
        loadTimeData.overrideValues({composeboxShowZps: false});
      });

  test('Selection is restored after selected match is removed', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new InputEvent('input'));

    let matches = [
      createSearchMatchForTesting({
        supportsDeletion: true,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: composeboxElement.$.input.value.trimStart(),
          matches,
        }));
    await microtasksFinished();
    assertTrue(await areMatchesShowing());

    let matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(1, matchEls.length);
    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // Remove the first match.
    matchEls[0]!.$.remove.click();
    const args = await searchboxHandler.whenCalled('deleteAutocompleteMatch');
    assertEquals(0, args[0]);
    assertEquals(1, searchboxHandler.getCallCount('deleteAutocompleteMatch'));

    searchboxHandler.reset();

    matches = [
      createSearchMatchForTesting({supportsDeletion: true}),
      createSearchMatchForTesting({
        supportsDeletion: true,
        fillIntoEdit: 'hello world 2',
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });

    composeboxElement.$.input.dispatchEvent(arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);

    // First match is selected
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world', composeboxElement.$.input.value);

    // By pressing 'Enter' on the button.
    const keydownEvent = (new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Enter',
    }));
    assertTrue(!!matchEls[0]!.$.remove);
    matchEls[0]!.$.remove.dispatchEvent(keydownEvent);
    assertTrue(keydownEvent.defaultPrevented);
    const keydownArgs =
        await searchboxHandler.whenCalled('deleteAutocompleteMatch');
    await microtasksFinished();
    assertEquals(0, keydownArgs[0]);
    assertEquals(1, searchboxHandler.getCallCount('deleteAutocompleteMatch'));

    matches = [createSearchMatchForTesting({
      supportsDeletion: true,
      fillIntoEdit: 'hello world 2',
    })];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('hello world 2', composeboxElement.$.input.value);
  });

  test('smart compose response added', async () => {
    createComposeboxElement();
    await microtasksFinished();

    // Add input.
    composeboxElement.$.input.value = 'smart ';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'smart ',
          matches: [],
          smartComposeInlineHint: 'compose',
        }));
    await microtasksFinished();

    assertEquals('compose', composeboxElement.getSmartComposeForTesting());
  });

  test('tab adds smart compose to input', async () => {
    createComposeboxElement();
    await microtasksFinished();
    // Autocomplete queried once when composebox is opened.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);

    // Add input.
    composeboxElement.$.input.value = 'smart ';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    // Autocomplete queried on input.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);

    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'smart ',
          matches: [],
          smartComposeInlineHint: 'compose',
        }));
    await microtasksFinished();

    const tabEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Tab',
    });

    composeboxElement.$.input.dispatchEvent(tabEvent);
    await microtasksFinished();
    assertTrue(tabEvent.defaultPrevented);

    assertEquals('smart compose', composeboxElement.$.input.value);
    // Autocomplete queried when smart compose accepted.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 3);
  });

  test('arrow up/down moves clears smart compose', async () => {
    loadTimeData.overrideValues({composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];

    // Add typed input
    composeboxElement.$.input.value = 'awesome';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'awesome',
          matches: matches,
          smartComposeInlineHint: 'compose',
        }));
    assertTrue(await areMatchesShowing());

    const smartCompose = $$<HTMLElement>(composeboxElement, '#smartCompose');
    assertTrue(!!smartCompose);

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });

    composeboxElement.$.input.dispatchEvent(arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);

    assertFalse(!!$$<HTMLElement>(composeboxElement, '#smartCompose'));
  });

  test('composebox does not open match when only file present', async () => {
    createComposeboxElement();

    assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        FileUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );
    await microtasksFinished();

    composeboxElement.$.submitContainer.click();
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(
        searchboxHandler.getCallCount('submitQuery'), 1,
        'submitQuery count should be 1');
    assertEquals(
        searchboxHandler.getCallCount('openAutocompleteMatch'), 0,
        'openAutocompleteMatch count should be 0');
  });

  test('composebox does not show when image is present', async () => {
    loadTimeData.overrideValues({
      composeboxShowZps: true,
      composeboxShowTypedSuggest: true,
      composeboxShowImageSuggest: false,
    });
    createComposeboxElement();
    // Autocomplete queried once when composebox is created.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);

    const matches = [createSearchMatchForTesting()];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches,
        }));
    assertTrue(await areMatchesShowing());

    // Upload an image.
    const id = generateZeroId();
    await uploadFileAndVerify(
        id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, FileUploadStatus.kProcessingSuggestSignalsReady, null);

    // Matches should not show when image is present.
    assertFalse(await areMatchesShowing());

    // Query autocomplete with image present to get verbatim match.
    composeboxElement.$.input.value = 'T';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);
  });

  test('composebox does not show verbatim match', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add zps input.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    let matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);
    let matchEl = matchEls[0];
    assertTrue(!!matchEl);
    // First match shows for zps.
    assertStyle(matchEl, 'display', 'block');

    // Add typed input
    composeboxElement.$.input.value = 'awesome';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'awesome',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing());

    matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);
    matchEl = matchEls[0];
    assertTrue(!!matchEl);
    // Verbatim match does not show for typed suggest.
    assertStyle(matchEl, 'display', 'none');
  });

  test('delete button removes match', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches,
          suggestionGroupsMap: {},
        }));

    assertTrue(await areMatchesShowing());

    const matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);
    const matchEl = matchEls[0];
    assertTrue(!!matchEl);

    const matchIndex = 0;
    const destinationUrl = 'http://google.com';
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    // By pressing 'Enter' on the button.
    const keydownEvent = (new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Enter',
    }));
    assertTrue(!!matchEl.$.remove);
    matchEl.$.remove.dispatchEvent(keydownEvent);
    assertTrue(keydownEvent.defaultPrevented);
    const keydownArgs =
        await searchboxHandler.whenCalled('deleteAutocompleteMatch');
    await microtasksFinished();
    assertEquals(matchIndex, keydownArgs[0]);
    assertEquals(destinationUrl, keydownArgs[1]);
    assertEquals(1, searchboxHandler.getCallCount('deleteAutocompleteMatch'));
    // Pressing the 'Enter' button doesn't accidentally trigger navigation.
    assertEquals(0, searchboxHandler.getCallCount('submitQuery'));
    searchboxHandler.reset();
    handler.reset();

    matchEl.$.remove.click();
    const clickArgs =
        await searchboxHandler.whenCalled('deleteAutocompleteMatch');
    await microtasksFinished();
    assertEquals(matchIndex, clickArgs[0]);
    assertEquals(destinationUrl, clickArgs[1]);
    assertEquals(1, searchboxHandler.getCallCount('deleteAutocompleteMatch'));
    // Clicking the button doesn't accidentally trigger navigation.
    assertEquals(0, searchboxHandler.getCallCount('submitQuery'));
  });

  test('composebox stops autocomplete when clearing input', async () => {
    createComposeboxElement();
    await microtasksFinished();

    // Autocomplete should be queried when the composebox is created.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 0);

    // Autocomplete complete should be queried when input is typed.
    composeboxElement.$.input.value = 'T';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);

    // Deleting to empty input should stop autocomplete before querying it
    // again.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 1);
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 3);
  });


  test('pasting valid files calls addFileContext', async () => {
    // Arrange.
    loadTimeData.overrideValues({'composeboxFileMaxCount': 5});
    createComposeboxElement();
    searchboxHandler.setPromiseResolveFor(
        ADD_FILE_CONTEXT_FN,
        {token: {low: BigInt(1), high: BigInt(2)}});

    const pngFile = new File(['foo'], 'foo.png', {type: 'image/png'});
    const pdfFile = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(pngFile);
    dataTransfer.items.add(pdfFile);
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    // Act.
    composeboxElement.$.input.dispatchEvent(pasteEvent);

    // Assert.
    // Check that addFileContext (ADD_FILE_CONTEXT_FN) was called twice.
    await waitForAddFileCallCount(2);
    const [[fileInfo1], [fileInfo2]] =
        searchboxHandler.getArgs(ADD_FILE_CONTEXT_FN);
    assertEquals('foo.png', fileInfo1.fileName);
    assertEquals('foo.pdf', fileInfo2.fileName);

    // Check that the default paste event was prevented.
    assertTrue(pasteEvent.defaultPrevented);
    assertEquals(1, metrics.count(CONTEXT_ADDED_NTP));
    assertEquals(
        1,
        metrics.count(
            CONTEXT_ADDED_NTP,
            /* COPY_PASTE */ 1));
  });

  test('pasting too many files records metric and prevents paste', async () => {
    // Arrange.
    const testInputState = {
      ...mockInputState,
      maxInstances: {
        [InputType.kBrowserTab]: 1,
        [InputType.kLensImage]: 1,
        [InputType.kLensFile]: 1,
      },
      maxTotalInputs: 2,
    };
    createComposeboxElement();
    searchboxCallbackRouterRemote.onInputStateChanged(testInputState);
    await microtasksFinished();

    searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
      return Promise.resolve({token: {low: BigInt(123), high: BigInt(0)}});
    });

    const pngFile1 = new File(['foo'], 'foo1.png', {type: 'image/png'});
    const pngFile2 = new File(['foo'], 'foo2.png', {type: 'image/png'});
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(pngFile1);
    dataTransfer.items.add(pngFile2);
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    // Act.
    composeboxElement.$.input.dispatchEvent(pasteEvent);
    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert.
    // Check that only one files were added.
    assertEquals(1, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // Check that the 'too many files' metric was recorded (Enum value 1).
    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 1));

    // Check that the paste event was prevented.
    assertTrue(pasteEvent.defaultPrevented);

    // Check whether the right error would show up.
    assertEquals(
        loadTimeData.getString('maxImagesReachedError'),
        composeboxElement.$.errorScrim.errorMessage);
  });

  test('pasting unsupported files fires validation error', async () => {
    // Arrange.
    createComposeboxElement();
    const txtFile = new File(['foo'], 'foo.txt', {type: 'text/plain'});
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(txtFile);
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    // Act.
    composeboxElement.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    // Assert.
    // Check that the correct error event was fired.
    assertEquals(
        loadTimeData.getString('composeFileTypesAllowedError'),
        composeboxElement.$.errorScrim.errorMessage);

    // Check that no files were added.
    assertEquals(0, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // Check that the paste event was prevented.
    assertTrue(pasteEvent.defaultPrevented);
  });

  test(
      'pasting only text does not call addFiles or prevent default',
      async () => {
        // Arrange.
        createComposeboxElement();
        const dataTransfer = new DataTransfer();
        dataTransfer.setData('text/plain', 'hello');
        const pasteEvent = new ClipboardEvent('paste', {
          clipboardData: dataTransfer,
          bubbles: true,
          cancelable: true,
          composed: true,
        });

        // Act.
        composeboxElement.$.input.dispatchEvent(pasteEvent);
        await microtasksFinished();

        // Assert.
        // Check that no files were added.
        assertEquals(0, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

        // Check the paste event was not prevented (onPaste_ returns early).
        assertFalse(pasteEvent.defaultPrevented);
      });

  test(
      'pasting mixed files is processed correctly ', async () => {
        // Arrange.
        createComposeboxElement();
        let i = 0;
        searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
          i += 1;
          return Promise.resolve(
              {token: {low: BigInt(i + 1), high: BigInt(i + 2)}});
        });
        const pngFile = new File(['foo'], 'foo.png', {type: 'image/png'});
        const pdfFile = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});

        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(pngFile);
        dataTransfer.items.add(pdfFile);
        const pasteEvent = new ClipboardEvent('paste', {
          clipboardData: dataTransfer,
          bubbles: true,
          cancelable: true,
          composed: true,
        });

        // Act.
        composeboxElement.$.input.dispatchEvent(pasteEvent);

        //Wait for both files to be processed (addFileContext called twice).
        await waitForAddFileCallCount(2);
        await microtasksFinished();

        // Assert.
        // Check if the Carousel received 2 files.
        const files = composeboxElement.$.carousel.files;
        assertEquals(files.length, 2);

        //  Check if the image was identified as an image.
        //  (has objectUrl) and the PDF was identified as a PDF (no objectUrl).
        const imageFile = files.find(f => f.type.includes('image'));
        const pdfFileInCarousel = files.find(f => f.type.includes('pdf'));

        // Ensure we found both.
        assertTrue(!!imageFile);
        assertTrue(!!pdfFileInCarousel);

        // Validate the image (it must have an objectUrl for preview).
        assertTrue(
            !!imageFile.objectUrl,
            'Image file should have an objectUrl for preview');

        // Validate the PDF (it must have null objectUrl to show the icon).
        assertEquals(
            pdfFileInCarousel.objectUrl, null,
            'PDF file should have null objectUrl');
      });

  test('uploading 6 valid files when limit is 5 uploads 5 and shows error', async () => {
    // Arrange.
    const testInputState = {
      ...mockInputState,
      maxTotalInputs: 5,
    };
    createComposeboxElement();
    searchboxCallbackRouterRemote.onInputStateChanged(testInputState);
    await microtasksFinished();

    let i = 0;
    searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
      i++;
      return Promise.resolve({low: BigInt(i), high: BigInt(0)});
    });

    const validFiles = Array.from({length: 6}, (_, i) =>
        new File(['foo'], `good${i}.png`, {type: 'image/png'}));

    const dataTransfer = new DataTransfer();
    validFiles.forEach(file => dataTransfer.items.add(file));

    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    // Act.
    composeboxElement.$.input.dispatchEvent(pasteEvent);

    await waitForAddFileCallCount(5);
    await microtasksFinished();

    // Assert.
    assertEquals(5, composeboxElement.$.carousel.files.length);

    assertEquals(
        loadTimeData.getString('maxImagesReachedError'),
        composeboxElement.$.errorScrim.errorMessage);

    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 1));
  });

  test('upload mixed files over limit prioritizes max files error and uploads valid ones', async () => {
    // Arrange.
    const testInputState = {
      ...mockInputState,
      maxInstances: {
        [InputType.kBrowserTab]: 1,
        [InputType.kLensImage]: 3,
        [InputType.kLensFile]: 1,
      },
      maxTotalInputs: 3,
    };
    createComposeboxElement();
    searchboxCallbackRouterRemote.onInputStateChanged(testInputState);
    await microtasksFinished();

    let i = 0;
      searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
        i++;
        return Promise.resolve({token: {low: BigInt(i), high: BigInt(0)}});
      });

      const files = [
        new File(['foo'], 'good1.png', {type: 'image/png'}),
        new File(['foo'], 'good2.png', {type: 'image/png'}),
        new File(['foo'], 'good3.png', {type: 'image/png'}),
        new File(['foo'], 'bad.txt', {type: 'text/plain'}),
      ];

      const dataTransfer = new DataTransfer();
      files.forEach(file => dataTransfer.items.add(file));

      const pasteEvent = new ClipboardEvent('paste', {
        clipboardData: dataTransfer,
        bubbles: true,
        cancelable: true,
        composed: true,
      });

      // Act.
      composeboxElement.$.input.dispatchEvent(pasteEvent);

      await waitForAddFileCallCount(3);
      await microtasksFinished();

      // Assert.
      assertEquals(3, composeboxElement.$.carousel.files.length);

      assertEquals(
          loadTimeData.getString('maxFilesReachedError'),
          composeboxElement.$.errorScrim.errorMessage);

      assertEquals(
          1,
          metrics.count(
              'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 1));
    });

  test(
      'uploading valid heif and invalid svg adds valid file and shows error',
      async () => {
        createComposeboxElement();


        let i = 0;
        searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
          i++;
          return Promise.resolve({low: BigInt(i), high: BigInt(0)});
        });

        const validFile = new File(['foo'], 'image.png', {type: 'image/png'});
        const invalidFile =
            new File(['bar'], 'icon.svg', {type: 'image/svg+xml'});

        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(validFile);
        dataTransfer.items.add(invalidFile);

        const pasteEvent = new ClipboardEvent('paste', {
          clipboardData: dataTransfer,
          bubbles: true,
          cancelable: true,
          composed: true,
        });

        composeboxElement.$.input.dispatchEvent(pasteEvent);

        await waitForAddFileCallCount(1);
        await microtasksFinished();

        assertEquals(1, composeboxElement.$.carousel.files.length);
        assertEquals(

            'image.png', composeboxElement.$.carousel.files[0]!.name);

        assertEquals(
            loadTimeData.getString('composeFileTypesAllowedError'),
            composeboxElement.$.errorScrim.errorMessage);
      });

  test('isCollapsible attribute sets expanding state when true', async () => {
    createComposeboxElement();
    const collapsibleBox = composeboxElement;
    (collapsibleBox as any).isCollapsible = true;
    document.body.appendChild(collapsibleBox);
    await collapsibleBox.updateComplete;

    const collapsibleInput = collapsibleBox.$.input;
    collapsibleBox.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should be expanded initially due to focus event');

    collapsibleBox.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await collapsibleBox.updateComplete;
    assertFalse(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should collapse on blur without text');

    collapsibleBox.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should expand on focus');

    // Set text and re-test blur logic
    collapsibleInput.value = 'some text';
    collapsibleInput.dispatchEvent(new Event('input'));
    await collapsibleBox.updateComplete;

    collapsibleBox.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should stay expanded on blur with text');
  });

  test('isCollapsible attribute sets expanded state with file', async () => {
    createComposeboxElement();
    (composeboxElement as any).isCollapsible = true;
    await microtasksFinished();

    composeboxElement.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await composeboxElement.updateComplete;
    assertTrue(
        composeboxElement.hasAttribute('expanding_'),
        'Collapsible should be expanded initially due to focus event');

    // Initially, carousel is not shown.
    assertFalse(composeboxElement.hasAttribute('show-file-carousel_'));

    // Set a thumbnail.
    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    searchboxCallbackRouterRemote.addFileContext(FAKE_TOKEN_STRING, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: true,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await microtasksFinished();

    // Assert thumbnail is shown.
    assertTrue(composeboxElement.hasAttribute('show-file-carousel_'));
    const fileCarousel = composeboxElement.$.carousel;
    assertTrue(!!fileCarousel);
    await microtasksFinished();

    composeboxElement.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await composeboxElement.updateComplete;
    assertTrue(
        composeboxElement.hasAttribute('expanding_'),
        'Collapsible should remain expanded on blur with file');

    // Delete the thumbnail.
    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);

    const removeImgButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    assertTrue(!!removeImgButton);
    removeImgButton.click();
    await microtasksFinished();

    // Focus the composebox again.
    composeboxElement.$.composebox.dispatchEvent(new FocusEvent('focusin'));
    await composeboxElement.updateComplete;
    assertTrue(
        composeboxElement.hasAttribute('expanding_'),
        'Collapsible should still expand when focused in');

    // Blur the composebox again.
    composeboxElement.$.composebox.dispatchEvent(
        new FocusEvent('focusout', {relatedTarget: document.body}));
    await composeboxElement.updateComplete;
    assertFalse(
        composeboxElement.hasAttribute('expanding_'),
        'Collapsible should collapse on blur with no file');
  });

  test('isCollapsible attribute sets expanded state when false', async () => {
    createComposeboxElement();
    const collapsibleBox = composeboxElement;
    const collapsibleInput = collapsibleBox.$.input;
    (collapsibleBox as any).isCollapsible = false;
    await collapsibleBox.updateComplete;

    // Blur the input first, since connectedCallback focuses it by default. This
    // ensures the component is in a state where it can be collapsed.
    collapsibleInput.blur();
    await collapsibleBox.updateComplete;

    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Non-collapsible should be expanded');
  });

  test('collapsible composebox collapses after query submitted', async () => {
    createComposeboxElement();
    const collapsibleBox = composeboxElement;
    const collapsibleInput = collapsibleBox.$.input;
    (collapsibleBox as any).isCollapsible = true;
    await collapsibleBox.updateComplete;

    collapsibleInput.focus();
    collapsibleInput.value = 'some text';
    collapsibleInput.dispatchEvent(new Event('input'));
    await collapsibleBox.updateComplete;
    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Collapsible should be expanded before submit');

    // Mock an autocomplete result to allow submission.
    const matches =
        [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'some text',
          matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await collapsibleBox.updateComplete;

    // Submit query.
    collapsibleBox.$.submitContainer.click();
    await collapsibleBox.updateComplete;
    await microtasksFinished();

    // Submit container should be disabled.
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'not-allowed');
    assertEquals('', collapsibleInput.value, 'Input should be cleared');
  });

  test('isCollapsible attribute sets expanded state when false', async () => {
    createComposeboxElement();
    const collapsibleBox = composeboxElement;
    const collapsibleInput = collapsibleBox.$.input;
    (collapsibleBox as any).isCollapsible = false;
    await collapsibleBox.updateComplete;

    // Blur the input first, since connectedCallback focuses it by default. This
    // ensures the component is in a state where it can be collapsed.
    collapsibleInput.blur();
    await collapsibleBox.updateComplete;

    assertTrue(
        collapsibleBox.hasAttribute('expanding_'),
        'Non-collapsible should be expanded');
  });

  test(
      '`autoSubmitVoiceSearchQuery` disabled updates input', async () => {
        // Set loadTimeData so that voice search does not auto submit.
        loadTimeData.overrideValues({
          autoSubmitVoiceSearchQuery: false,
          expandedComposeboxShowVoiceSearch: true,
          steadyComposeboxShowVoiceSearch: true,
          composeboxShowZps: true,  // For predictable queryAutocomplete count.
        });
        createComposeboxElement();
        await microtasksFinished();
        searchboxHandler.reset();

        const voiceQuery = 'hello';
        composeboxElement.$.voiceSearch.dispatchEvent(new CustomEvent(
            'voice-search-final-result',
            {detail: voiceQuery, bubbles: true, composed: true}));
        await microtasksFinished();

        // Assertions.
        assertEquals(composeboxElement.$.input.value, voiceQuery);
        // Ensure the query isn't auto submitted.
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
        // Ensure autocomplete is queried since there's input in the composebox.
        assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
        assertEquals(
            voiceQuery, searchboxHandler.getArgs('queryAutocomplete')[0][0]);

        // Mock an autocomplete result so that submitQuery assertion passes.
        const matches =
            [createSearchMatchForTesting({allowedToBeDefaultMatch: true})];
        searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: voiceQuery,
              matches,
            }));
        await searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertFalse(composeboxElement.$.input.hidden);
        assertEquals(
            composeboxElement.shadowRoot.activeElement,
            composeboxElement.$.input);

        // Simulate submit button click.
        composeboxElement.$.submitContainer.dispatchEvent(
            new FocusEvent('focusin'));
        composeboxElement.$.submitContainer.click();

        // Since a match is selected, openAutocompleteMatch is called instead of
        // submitQuery.
        await searchboxHandler.whenCalled('openAutocompleteMatch');
        await microtasksFinished();

        assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
        assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 1);
        const [index] = searchboxHandler.getArgs('openAutocompleteMatch')[0];
        assertEquals(index, 0);
      });

  test(
      '`autoSubmitVoiceSearchQuery` enabled submits w/o querying autocomplete',
      async () => {
        // Set loadTimeData so that voice search does auto submit.
        loadTimeData.overrideValues({
          autoSubmitVoiceSearchQuery: true,
          expandedComposeboxShowVoiceSearch: true,
          steadyComposeboxShowVoiceSearch: true,
          composeboxShowZps: true,  // For predictable queryAutocomplete count.
        });
        createComposeboxElement();
        await microtasksFinished();
        searchboxHandler.reset();

        const voiceSearchActionPromise =
            eventToPromise('voice-search-action', composeboxElement);
        const voiceQuery = 'hello';
        composeboxElement.$.voiceSearch.dispatchEvent(new CustomEvent(
            'voice-search-final-result',
            {detail: voiceQuery, bubbles: true, composed: true}));

        // Assert event fired.
        const voiceSearchActionEvent = await voiceSearchActionPromise;
        assertEquals(
            VoiceSearchAction.QUERY_SUBMITTED,
            voiceSearchActionEvent.detail.value);
        await microtasksFinished();

        assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 0);
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);
        assertEquals(voiceQuery, searchboxHandler.getArgs('submitQuery')[0][0]);
      });

  test('onInputStateChanged updates inputState', async () => {
    createComposeboxElement();
    const inputState = {
      allowedModels: [],
      allowedTools: [],
      allowedInputTypes: [],
      activeModel: 0,
      activeTool: 0,
      disabledModels: [],
      disabledTools: [],
      disabledInputTypes: [],
      inputTypeConfigs: [],
      toolConfigs: [],
      modelConfigs: [],
      toolsSectionConfig: null,
      modelSectionConfig: null,
      hintText: '',
      maxInstances: {},
      maxTotalInputs: 0,
    } as InputState;
    searchboxCallbackRouterRemote.onInputStateChanged(inputState);
    await microtasksFinished();
    assertDeepEquals((composeboxElement as any).inputState_, inputState);
  });

  suite('Context menu', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        composeboxShowRecentTabChip: true,
        composeboxShowContextMenu: true,
      });
    });

    test('context button visible', () => {
      createComposeboxElement();

      const contextMenuButton = $$(composeboxElement, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
    });

    test('add tab context', async () => {
      createComposeboxElement();
      await addTab();
    });

    test('add tab context fails', async () => {
      createComposeboxElement();
      // Set the promise to reject to simulate a failure.
      searchboxHandler.setResultMapperFor(ADD_TAB_CONTEXT_FN, () => {
        return Promise.reject(FileUploadErrorType.kBrowserProcessingError);
      });

      // Assert no files.
      assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

      const contextMenuButton = $$(composeboxElement, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
      const sampleTabTitle = 'Sample Tab';
      let contextAdded = false;
      const callback = (_file: any) => {
        contextAdded = true;
      };

      contextMenuButton.dispatchEvent(new CustomEvent('add-tab-context', {
        detail: {id: 1, title: sampleTabTitle, onContextAdded: callback},
        bubbles: true,
        composed: true,
      }));

      await searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
      await microtasksFinished();

      // Assert callback was not called and no files in carousel.
      assertFalse(contextAdded);
      assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

      assertEquals(
          loadTimeData.getString('composeboxFileUploadFailed'),
          composeboxElement.$.errorScrim.errorMessage);
    });

    test('add file context fails', async () => {
      loadTimeData.overrideValues({composeboxShowPdfUpload: true});
      createComposeboxElement();
      // Set the promise to reject to simulate a failure.
      searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
        return Promise.reject(FileUploadErrorType.kBrowserProcessingError);
      });

      // Assert no files.
      assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

      // Act.
      const dataTransfer = new DataTransfer();
      const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
      dataTransfer.items.add(file);
      composeboxElement.$.fileInputs.$.fileInput.files = dataTransfer.files;
      composeboxElement.$.fileInputs.$.fileInput.dispatchEvent(
          new Event('change'));

      await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
      await microtasksFinished();

      // Assert no files in carousel.
      assertFalse(!!$$<HTMLElement>(composeboxElement, '#carousel'));

      assertEquals(
          loadTimeData.getString('composeboxFileUploadFailed'),
          composeboxElement.$.errorScrim.errorMessage);
    });

    test('setSearchContext sets input and queries autocomplete', async () => {
      loadTimeData.overrideValues({composeboxShowZps: true});
      composeboxElement = new ComposeboxElement();
      composeboxElement.searchboxNextEnabled = true;
      document.body.appendChild(composeboxElement);

      await microtasksFinished();

      // Autocomplete waits
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 0);

      const context = {
        input: 'hello world',
        files: [],
        attachments: [],
        toolMode: 0,
      };
      composeboxElement.addSearchContext(context);
      await microtasksFinished();

      // Check that input and lastQueriedInput are set.
      assertEquals(composeboxElement.getText(), 'hello world');
      assertEquals((composeboxElement as any).lastQueriedInput_, 'hello world');
      // Autocomplete should be queried again.
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
    });

    test('tab changes calls getRecentTabs', async () => {
      createComposeboxElement();
      loadTimeData.overrideValues({
        realboxLayoutMode: 'TallTopContext',
        composeboxShowRecentTabChip: true,
      });
      const sampleTabs = [
        {
          tabId: 1,
          title: 'Sample Tab 1',
          url: 'https://example.com/1',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(1)},
        },
        {
          tabId: 2,
          title: 'Sample Tab 2',
          url: 'https://example.com/2',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(2)},
        },
      ];

      searchboxHandler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

      const entrypointAndMenu = composeboxElement.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-and-menu');
      assertTrue(!!entrypointAndMenu, 'contextual-entrypoint-and-menu');
      const contextMenuEntrypoint = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextMenuEntrypoint, 'contextual entrypoint button');
      const entrypointButton =
          contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
              '#entrypoint');
      assertTrue(!!entrypointButton, 'Entrypoint button');
      entrypointButton.click();

      await microtasksFinished();

      // There is an initial call to `getRecentTabs` on entrypoint click.
      assertEquals(searchboxHandler.getCallCount('getRecentTabs'), 1);

      // Assert another call to `getRecentTabs` is made on tab changes.
      searchboxCallbackRouterRemote.onTabStripChanged();
      await searchboxCallbackRouterRemote.$.flushForTesting();
      assertEquals(searchboxHandler.getCallCount('getRecentTabs'), 2);
    });
  });

  test('autocomplete queried when autochip removed', async () => {
    createComposeboxElement();
    await microtasksFinished();

    // Autocomplete queried once on load.
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
    searchboxHandler.reset();
    searchboxHandler.setPromiseResolveFor(
        ADD_TAB_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    const tab = {
      tabId: 1,
      title: 'Tab 1',
      url: 'https://example.com/1',
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)},
    } as any as TabInfo;

    // Add autochip.
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tab);
    await microtasksFinished();

    // Should have cleared matches.
    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 1);
    searchboxHandler.reset();

    // Remove autochip.
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(null);
    await microtasksFinished();

    // Autocomplete should be queried again when an auto chip is removed.
    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 2);
    assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 2);
  });

  test('matches cleared when new autochip added', async () => {
    createComposeboxElement();
    await microtasksFinished();

    searchboxHandler.reset();
    searchboxHandler.setPromiseResolveFor(
        ADD_TAB_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    const tab = {
      tabId: 1,
      title: 'Tab 1',
      url: 'https://example.com/1',
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)},
    } as any as TabInfo;

    // Add valid autochip.
    searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tab);
    await microtasksFinished();

    // Should clear matches when a new autochip is added.
    assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 1);
  });

  test(
      'autocomplete not requeried if no autochip to start and updated with null',
      async () => {
        createComposeboxElement();
        await microtasksFinished();

        // Autocomplete queried once on load.
        assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);

        // Remove autochip when none exists.
        searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(null);
        await microtasksFinished();

        // Autocomplete should not be queried again when there was no autochip
        // to start, and an update comes with a null tab.
        assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
        assertEquals(searchboxHandler.getCallCount('stopAutocomplete'), 0);
      });

  test('when flag enabled, adds tab context of ghost file', async () => {
    createComposeboxElement();
    document.body.appendChild(composeboxElement);
    composeboxElement.shouldShowGhostFiles = true;

    await addTab();

    await composeboxElement.updateComplete;
    await microtasksFinished();

    assertTrue(
        composeboxElement.getNumOfFilesForTesting() === 1,
        'Tab should be added');

    const bad_token = FAKE_TOKEN_STRING_2;
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        bad_token,
        FileUploadStatus.kUploadSuccessful,
        null,
    );
    await composeboxElement.updateComplete;
    await microtasksFinished();
    assertTrue(
        composeboxElement.getNumOfFilesForTesting() === 2,
        'Ghost file should be added');
  });

  test('does not add tab context of ghost file', async () => {
    createComposeboxElement();
    document.body.appendChild(composeboxElement);
    composeboxElement.shouldShowGhostFiles = false;

    await addTab();
    await composeboxElement.updateComplete;
    await microtasksFinished();


    assertTrue(
        composeboxElement.getNumOfFilesForTesting() === 1,
        'Tab should be added');
    const bad_token = FAKE_TOKEN_STRING_2;
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        bad_token,
        FileUploadStatus.kUploadSuccessful,
        null,
    );
    await composeboxElement.updateComplete;
    await microtasksFinished();
    assertTrue(
        composeboxElement.getNumOfFilesForTesting() === 1,
        'Ghost file should not be added');
  });
});
