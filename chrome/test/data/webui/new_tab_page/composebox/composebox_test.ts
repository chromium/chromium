// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SelectedFileInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ComposeboxElement, ComposeboxProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {FileUploadErrorType, FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {RecentTabChipElement} from 'chrome://resources/cr_components/composebox/recent_tab_chip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
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

  setup(() => {
     loadTimeData.overrideValues({
    'composeboxImageFileTypes': 'image/avif,image/bmp,image/jpeg,image/png,image/webp,image/heif,image/heic',
    'composeboxAttachmentFileTypes': '.pdf,application/pdf',
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
    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));
    searchboxCallbackRouterRemote =
        ComposeboxProxyImpl.getInstance()
            .searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
  });

  function createComposeboxElement() {
    composeboxElement = new ComposeboxElement();
    document.body.appendChild(composeboxElement);
  }

  async function getRecentTabChip(): Promise<HTMLElement|null> {
    const contextElement = composeboxElement.$.context;
    await microtasksFinished();
    await contextElement.updateComplete;
    return contextElement.shadowRoot.querySelector<HTMLElement>(
        '#recentTabChip');
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

  function getInputForFileType(fileType: string): HTMLInputElement {
    return fileType === 'application/pdf' ?
        composeboxElement.$.context.$.fileInput :
        composeboxElement.$.context.$.imageInput;
  }

  function getMockFileChangeEventForType(fileType: string): Event {
    if (fileType === 'application/pdf') {
      return new Event('change');
    }

    const mockFileChange = new Event('change', {bubbles: true});
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: composeboxElement.$.context.$.imageInput,
    });
    return mockFileChange;
  }

  function createAutocompleteMatch(): AutocompleteMatch {
    return {
      isHidden: false,
      a11yLabel: '',
      actions: [],
      allowedToBeDefaultMatch: false,
      isSearchType: false,
      isEnterpriseSearchAggregatorPeopleType: false,
      swapContentsAndDescription: false,
      supportsDeletion: false,
      suggestionGroupId: -1,  // Indicates a missing suggestion group Id.
      contents: '',
      contentsClass: [{offset: 0, style: 0}],
      description: '',
      descriptionClass: [{offset: 0, style: 0}],
      destinationUrl: {url: ''},
      inlineAutocompletion: '',
      fillIntoEdit: '',
      iconPath: '',
      iconUrl: {url: ''},
      imageDominantColor: '',
      imageUrl: '',
      isNoncannedAimSuggestion: false,
      removeButtonA11yLabel: '',
      type: '',
      isRichSuggestion: false,
      isWeatherAnswerSuggestion: null,
      answer: null,
      tailSuggestCommonPrefix: null,
      hasInstantKeyword: false,
      keywordChipHint: '',
      keywordChipA11y: '',
    };
  }

  function createAutocompleteResult(
      modifiers: Partial<AutocompleteResult> = {}): AutocompleteResult {
    const base: AutocompleteResult = {
      input: '',
      matches: [],
      suggestionGroupsMap: {},
      smartComposeInlineHint: null,
    };

    return Object.assign(base, modifiers);
  }

  function createSearchMatch(modifiers: Partial<AutocompleteMatch> = {}):
      AutocompleteMatch {
    return Object.assign(
        createAutocompleteMatch(), {
          isSearchType: true,
          contents: 'hello world',
          destinationUrl: {url: 'https://www.google.com/search?q=hello+world'},
          fillIntoEdit: 'hello world',
          type: 'search-suggest',
        },
        modifiers);
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
    assertFalse(!!$$<HTMLElement>(composeboxElement.$.context, '#carousel'));

    searchboxHandler.setResultFor(ADD_FILE_CONTEXT_FN,
                                  Promise.resolve({token: token}));

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
    const files = composeboxElement.$.context.$.carousel.files;
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

  test('clear functionality', async () => {
    loadTimeData.overrideValues({composeboxShowSubmit: true});
    createComposeboxElement();
    searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN,
        Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

    // Check submit button disabled.
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'default');
    // Add input.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
    composeboxElement.$.context.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.context.$.fileInput.dispatchEvent(new Event('change'));

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Check submit button enabled and file uploaded.
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'pointer');
    assertEquals(composeboxElement.$.context.$.carousel.files.length, 1);

    // Clear input.
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await microtasksFinished();

    // Assert
    assertEquals(searchboxHandler.getCallCount('clearFiles'), 1);

    // Check submit button disabled and files empty.
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'default');
    assertFalse(!!$$<HTMLElement>(composeboxElement.$.context, '#carousel'));

    // Close composebox.
    const whenCloseComposebox =
        eventToPromise('close-composebox', composeboxElement);
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await whenCloseComposebox;
  });

  test('upload image', async () => {
    createComposeboxElement();
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'default');
    const token = {low: BigInt(1), high: BigInt(2)};
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'pointer');
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
    const deletedId = composeboxElement.$.context.$.carousel.files[0]!.uuid;
    composeboxElement.$.context.$.carousel.dispatchEvent(
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
        {composeboxShowZps: true, composeboxShowImageSuggestions: false});
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
    assertFalse(!!$$<HTMLElement>(composeboxElement.$.context, '#carousel'));
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
    assertFalse(!!$$<HTMLElement>(composeboxElement.$.context, '#carousel'));
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
              !!$$<HTMLElement>(composeboxElement.$.context, '#carousel'));
        });
  });

  test('upload pdf', async () => {
    createComposeboxElement();
    searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN,
        Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

    // Assert no files.
    assertFalse(!!$$<HTMLElement>(composeboxElement.$.context, '#carousel'));

    // Arrange.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    composeboxElement.$.context.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.context.$.fileInput.dispatchEvent(
        new Event('change'));

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert one pdf file.
    const files = composeboxElement.$.context.$.carousel.files;
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
  });

  test('delete file', async () => {
    loadTimeData.overrideValues({composeboxFileMaxCount: 5});
    createComposeboxElement();
    let i = 0;
    searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
      i += 1;
      return Promise.resolve(
          {token: {low: BigInt(i + 1), high: BigInt(i + 2)}});
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
      value: composeboxElement.$.context.$.fileInput,
    });

    composeboxElement.$.context.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.context.$.fileInput.dispatchEvent(mockFileChange);

    await waitForAddFileCallCount(2);
    await composeboxElement.updateComplete;
    await microtasksFinished();

    // Assert two files are present initially.
    assertEquals(composeboxElement.$.context.$.carousel.files.length, 2);

    // Act.
    const deletedId = composeboxElement.$.context.$.carousel.files[0]!.uuid;
    composeboxElement.$.context.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();

    // Assert.
    assertEquals(composeboxElement.$.context.$.carousel.files.length, 1);
    assertEquals(searchboxHandler.getCallCount('deleteContext'), 1);
    const [idArg] = searchboxHandler.getArgs('deleteContext');
    assertEquals(idArg, deletedId);
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
    await handler.whenCalled('handleLensButtonClick');
    assertEquals(1, handler.getCallCount('handleLensButtonClick'));
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
    const fileCarousel = composeboxElement.$.context.$.carousel;
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
    const [idArg] = searchboxHandler.getArgs('deleteContext');
    assertEquals(idArg, FAKE_TOKEN_STRING);
    // The carousel is removed from the DOM when there are no files, so
    // assert its absence.
    assertFalse(
        !!composeboxElement.$.context.shadowRoot.querySelector('#carousel'));
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
    const fileCarousel = composeboxElement.$.context.$.carousel;
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

  test('image upload button clicks file input', async () => {
    loadTimeData.overrideValues({
      'composeboxShowContextMenu': false,
    });
    createComposeboxElement();
    const imageUploadEventPromise = eventToPromise(
        'click', composeboxElement.$.context.$.imageInput);
    composeboxElement.$.context.$.imageUploadButton.click();

    // Assert.
    await imageUploadEventPromise;
  });

  test('file upload button clicks file input', async () => {
    loadTimeData.overrideValues({
      'composeboxShowPdfUpload': true,
      'composeboxShowContextMenu': false,
    });
    createComposeboxElement();
    const fileUploadClickEventPromise = eventToPromise(
        'click', composeboxElement.$.context.$.fileInput);
    composeboxElement.$.context.$.fileUploadButton.click();

    // Assert.
    await fileUploadClickEventPromise;
  });

  test('disabling file upload does not show fileUploadButton', async () => {
    loadTimeData.overrideValues({'composeboxShowPdfUpload': false});
    createComposeboxElement();
    await composeboxElement.updateComplete;

    // Assert
    assertFalse(
        !!composeboxElement.$.context.shadowRoot.querySelector(
            '#fileUploadButton'));
  });

  test('file upload buttons disabled when max files uploaded', async () => {
    loadTimeData.overrideValues({'composeboxFileMaxCount': 1});
    loadTimeData.overrideValues({'composeboxShowPdfUpload': true});
    createComposeboxElement();
    searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN,
        Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

    // File upload buttons are not disabled when there are no files.
    assertFalse(composeboxElement.$.context.$.fileUploadButton.disabled);
    assertFalse(composeboxElement.$.context.$.imageUploadButton.disabled);

    // Arrange.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    composeboxElement.$.context.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.context.$.fileInput.dispatchEvent(new Event('change'));

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert.
    assertTrue(composeboxElement.$.context.$.fileUploadButton.disabled);
    assertTrue(composeboxElement.$.context.$.imageUploadButton.disabled);
  });

  test(
      'inputs disabled based on file count and create image mode', async () => {
        loadTimeData.overrideValues({
          'composeboxFileMaxCount': 1,
          'composeboxShowCreateImageButton': true,
        });

        createComposeboxElement();
        await microtasksFinished();

        searchboxHandler.setResultFor(
            ADD_FILE_CONTEXT_FN,
            Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

        // Upload a PDF file. `inputsDisabled` should be true.
        const pdfFile = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(pdfFile);
        composeboxElement.$.context.$.fileInput.files = dataTransfer.files;
        composeboxElement.$.context.$.fileInput.dispatchEvent(
            new Event('change'));

        await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
        await microtasksFinished();
        assertTrue(composeboxElement.$.context['inputsDisabled_']);

        // Delete the file. `inputsDisabled` should be false.
        const deletedId = composeboxElement.$.context.$.carousel.files[0]!.uuid;
        composeboxElement.$.context.$.carousel.dispatchEvent(new CustomEvent(
            'delete-file',
            {detail: {uuid: deletedId}, bubbles: true, composed: true}));
        await microtasksFinished();
        assertFalse(composeboxElement.$.context['inputsDisabled_']);
        searchboxHandler.resetResolver(ADD_FILE_CONTEXT_FN);
        searchboxHandler.setResultFor(
            ADD_FILE_CONTEXT_FN,
            Promise.resolve({token: {low: BigInt(3), high: BigInt(4)}}));

        // Upload an image file. `inputsDisabled` should be false.
        const imageFile = new File(['foo'], 'foo.png', {type: 'image/png'});
        const dataTransfer2 = new DataTransfer();
        dataTransfer2.items.add(imageFile);

        const imageInput = composeboxElement.$.context.$.imageInput;
        imageInput.files = dataTransfer2.files;
        imageInput.dispatchEvent(new Event('change'));

        await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
        await microtasksFinished();
        assertFalse(composeboxElement.$.context['inputsDisabled_']);

        // Enter create image mode. `inputsDisabled` should be true.
        composeboxElement.$.context['inCreateImageMode_'] = true;
        await composeboxElement.$.context.updateComplete;
        assertTrue(composeboxElement.$.context['inputsDisabled_']);

        // Exit create image mode. `inputsDisabled` should be false.
        composeboxElement.$.context['inCreateImageMode_'] = false;
        await composeboxElement.$.context.updateComplete;
        assertFalse(composeboxElement.$.context['inputsDisabled_']);
      });

  test('session abandoned on esc click', async () => {
    // Arrange.
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
  });

  test('escape key behavior with suggestions', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    const matches = [
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({matches}));
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
  });

  test('submit button click leads to handler called', async () => {
    createComposeboxElement();
    // Assert.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);

    // Arrange.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const matches = [createSearchMatch({allowedToBeDefaultMatch: true})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
    const matches = [createSearchMatch({allowedToBeDefaultMatch: true})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          matches: matches,
        }));
    await microtasksFinished();
    assertFalse(composeboxDropdown.hidden);

    // If multiple context files are added, the dropdown should hide.
    composeboxElement.$.context.dispatchEvent(
      new CustomEvent('on-context-files-changed', {
        detail: {files: 2},
      }));
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
      createSearchMatch(
          {fillIntoEdit: 'hello world 1', allowedToBeDefaultMatch: true}),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
      createSearchMatch({fillIntoEdit: 'hello world 3'}),
      createSearchMatch({fillIntoEdit: 'hello world 4'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      createSearchMatch(
          {fillIntoEdit: 'hello world 1', allowedToBeDefaultMatch: true}),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
      createSearchMatch({fillIntoEdit: 'hello world 3'}),
      createSearchMatch({fillIntoEdit: 'hello world 4'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          matches: matches,
          input: 'Test',
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown.hidden);

    // If context files are added, the dropdown should no longer be visible.
    composeboxElement.$.context.dispatchEvent(
      new CustomEvent('on-context-files-changed', {
        detail: {files: 1},
      }));
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
          createSearchMatch(),
        ];
        searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResult({
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
    composeboxElement.$.context.dispatchEvent(
        new CustomEvent('set-create-image-mode', {
          detail: {inCreateImageMode: true},
        }));
    await microtasksFinished();
    assertEquals(handler.getCallCount('setCreateImageMode'), 1);

    // Upload an image file. `inputsDisabled` should be false.
    const id = generateZeroId();
    await uploadFileAndVerify(
        id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, FileUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    // TODO(crbug.com/452957831): Create test browser proxy for the composebox
    // handler so we can assert the parameters this function was called with.
    assertEquals(handler.getCallCount('setCreateImageMode'), 2);

    // Deleting the image should call setCreateImageMode again but with
    // imagePresent false.
    const deletedId = composeboxElement.$.context.$.carousel.files[0]!.uuid;
    composeboxElement.$.context.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();
    assertEquals(handler.getCallCount('setCreateImageMode'), 3);
  });

  test('arrow up/down moves selection / focus', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add zps input.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    const matches = [
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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

  test('Selection is restored after selected match is removed', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement();
    await microtasksFinished();

    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new InputEvent('input'));

    let matches = [
      createSearchMatch({
        supportsDeletion: true,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      createSearchMatch({supportsDeletion: true}),
      createSearchMatch({
        supportsDeletion: true,
        fillIntoEdit: 'hello world 2',
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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

    matches = [createSearchMatch({
      supportsDeletion: true,
      fillIntoEdit: 'hello world 2',
    })];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
        createAutocompleteResult({
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
        createAutocompleteResult({
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
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];

    // Add typed input
    composeboxElement.$.input.value = 'awesome';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
    const token = {low: BigInt(1), high: BigInt(2)};
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    composeboxElement.$.submitContainer.click();
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);
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

    const matches = [createSearchMatch()];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
        createAutocompleteResult({
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
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: 'hello world 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
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
    const destinationUrl = {url: 'http://google.com'};
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

  test('placeholder text is updated when in deep search mode', async () => {
    // Assert initial placeholder text.
    assertEquals(
        loadTimeData.getString('searchboxComposePlaceholder'),
        composeboxElement.$.input.placeholder);

    // Deep search mode enabled.
    composeboxElement.$.context.dispatchEvent(
        new CustomEvent('set-deep-search-mode', {
          detail: {inDeepSearchMode: true},
        }));
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('composeDeepSearchPlaceholder'),
        composeboxElement.$.input.placeholder);

    // Deep search mode disabled.
    composeboxElement.$.context.dispatchEvent(
        new CustomEvent('set-deep-search-mode', {
          detail: {inDeepSearchMode: false},
        }));
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('searchboxComposePlaceholder'),
        composeboxElement.$.input.placeholder);
  });

  test('placeholder text is updated when in create image mode', async () => {
    // Assert initial placeholder text.
    assertEquals(
        loadTimeData.getString('searchboxComposePlaceholder'),
        composeboxElement.$.input.placeholder);

    // Create image mode enabled.
    composeboxElement.$.context.dispatchEvent(
        new CustomEvent('set-create-image-mode', {
          detail: {inCreateImageMode: true},
        }));
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('composeCreateImagePlaceholder'),
        composeboxElement.$.input.placeholder);

    // Create image mode disabled.
    composeboxElement.$.context.dispatchEvent(
        new CustomEvent('set-create-image-mode', {
          detail: {inCreateImageMode: false},
        }));
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('searchboxComposePlaceholder'),
        composeboxElement.$.input.placeholder);
  });

  test('pasting valid files calls addFileContext', async () => {
    // Arrange.
    loadTimeData.overrideValues({'composeboxFileMaxCount': 5});
    createComposeboxElement();
    searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN,
        Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

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
  });

  test('pasting too many files records metric and prevents paste', async () => {
    // Arrange.
    loadTimeData.overrideValues({'composeboxFileMaxCount': 1});
    createComposeboxElement();

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
    const errorEventPromise =
        eventToPromise('on-file-validation-error', composeboxElement.$.context);

    // Act.
    composeboxElement.$.input.dispatchEvent(pasteEvent);
    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert.
    // Check that only one files were added.
    assertEquals(1, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // Check that the "too many files" metric was recorded (Enum value 1).
    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 1));

    // Check that the paste event was prevented.
    assertTrue(pasteEvent.defaultPrevented);

    // Check whether the right error would show up.
    const errorEvent = await errorEventPromise;
    assertEquals(
        loadTimeData.getString('maxFilesReachedError'),
        errorEvent.detail.errorMessage);
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

    // Listen for the error event.
    const errorEventPromise =
        eventToPromise('on-file-validation-error', composeboxElement.$.context);

    // Act.
    composeboxElement.$.input.dispatchEvent(pasteEvent);

    // Assert.
    // Check that the correct error event was fired.
    const errorEvent = await errorEventPromise;
    assertEquals(
        loadTimeData.getString('composeFileTypesAllowedError'),
        errorEvent.detail.errorMessage);

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
      'pasting mixed files is processesed correctly ',
      async () => {
        // Arrange.
        loadTimeData.overrideValues({'composeboxFileMaxCount': 5});
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
        const files = composeboxElement.$.context.$.carousel.files;
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
    loadTimeData.overrideValues({'composeboxFileMaxCount': 5});
    createComposeboxElement();

    let i = 0;
    searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
      i++;
      return Promise.resolve({token: {low: BigInt(i), high: BigInt(0)}});
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

    const errorEventPromise =
        eventToPromise('on-file-validation-error', composeboxElement.$.context);

    // Act.
    composeboxElement.$.input.dispatchEvent(pasteEvent);

    await waitForAddFileCallCount(5);
    await microtasksFinished();

    // Assert.
    assertEquals(5, composeboxElement.$.context.$.carousel.files.length);

    const errorEvent = await errorEventPromise;
    assertEquals(
        loadTimeData.getString('maxFilesReachedError'),
        errorEvent.detail.errorMessage);

    assertEquals(
        1,
        metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 1));
  });

  test('upload mixed files over limit prioritizes max files error and uploads valid ones', async () => {
    // Arrange.
    loadTimeData.overrideValues({'composeboxFileMaxCount': 3});
      createComposeboxElement();

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

      const errorEventPromise =
          eventToPromise('on-file-validation-error', composeboxElement.$.context);

      // Act.
      composeboxElement.$.input.dispatchEvent(pasteEvent);

      await waitForAddFileCallCount(3);
      await microtasksFinished();

      // Assert.
      assertEquals(3, composeboxElement.$.context.$.carousel.files.length);

      const errorEvent = await errorEventPromise;
      assertEquals(
          loadTimeData.getString('maxFilesReachedError'),
          errorEvent.detail.errorMessage);

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
          return Promise.resolve({token: {low: BigInt(i), high: BigInt(0)}});
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

        const errorEventPromise = eventToPromise(
            'on-file-validation-error', composeboxElement.$.context);

        composeboxElement.$.input.dispatchEvent(pasteEvent);

        await waitForAddFileCallCount(1);
        await microtasksFinished();

        assertEquals(1, composeboxElement.$.context.$.carousel.files.length);
        assertEquals(

            'image.png', composeboxElement.$.context.$.carousel.files[0]!.name);

        const errorEvent = await errorEventPromise;

        assertEquals(
            loadTimeData.getString('composeFileTypesAllowedError'),
            errorEvent.detail.errorMessage);
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
    const fileCarousel = composeboxElement.$.context.$.carousel;
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
    const matches = [createSearchMatch({allowedToBeDefaultMatch: true})];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          input: 'some text',
          matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await collapsibleBox.updateComplete;

    // Submit query.
    collapsibleBox.$.submitContainer.click();
    await collapsibleBox.updateComplete;
    await microtasksFinished();

    assertStyle(composeboxElement.$.submitContainer, 'cursor', 'default');
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

  suite('Context menu', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        composeboxShowRecentTabChip: true,
        composeboxShowContextMenu: true,
      });
    });

    test('context button replaces upload container', () => {
      createComposeboxElement();

      const uploadContainer = $$(
          composeboxElement.$.context, '#uploadContainer');
      assertFalse(!!uploadContainer);
      const contextMenuButton = $$(
          composeboxElement.$.context, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
    });

    test('add tab context', async () => {
      createComposeboxElement();
      searchboxHandler.setResultFor(
          ADD_TAB_CONTEXT_FN,
          Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

      // Assert no files.
      assertFalse(!!$$<HTMLElement>(composeboxElement.$.context, '#carousel'));

      const contextMenuButton = $$(
          composeboxElement.$.context, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
      const sampleTabTitle = 'Sample Tab';
      contextMenuButton.dispatchEvent(new CustomEvent('add-tab-context', {
        detail: {id: 1, title: sampleTabTitle},
        bubbles: true,
        composed: true,
      }));

      await searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
      await microtasksFinished();
      const files = composeboxElement.$.context.$.carousel.files;
      assertEquals(files.length, 1);
      assertEquals(files[0]!.type, 'tab');
      assertEquals(files[0]!.name, sampleTabTitle);
    });

    test('recent tab chip shows first available suggestion', async () => {
      loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
      const tabInfo1 = {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'https://www.google.com/search?q=foo'},
        showInRecentTabChip: false,
      };
      const tabInfo2 = {
        tabId: 2,
        title: 'Tab 2',
        url: {url: 'https://www.example.com'},
        showInRecentTabChip: true,
      };
      const tabInfo3 = {
        tabId: 3,
        title: 'Tab 3',
        url: {url: 'https://www.chromium.org'},
        showInRecentTabChip: true,
      };
      searchboxHandler.setResultFor(
          'getRecentTabs',
          Promise.resolve({tabs: [tabInfo1, tabInfo2, tabInfo3]}));
      createComposeboxElement();
      await microtasksFinished();

      // Add zps input.
      composeboxElement.$.input.value = '';
      composeboxElement.$.input.dispatchEvent(new Event('input'));
      await microtasksFinished();

      const composeboxDropdown =
          composeboxElement.shadowRoot.querySelector<HTMLElement>('#matches');
      assertTrue(!!composeboxDropdown);

      // Recent tab chip should not show for no matches.
      assertTrue(composeboxDropdown.hidden);
      let recentTabChip = await getRecentTabChip();
      assertFalse(!!recentTabChip);

      const matches = [
        createSearchMatch(),
        createSearchMatch({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResult({
            matches: matches,
          }));
      await microtasksFinished();

      // Dropdown should show when matches are available.
      assertFalse(composeboxDropdown.hidden);
      recentTabChip = await getRecentTabChip();
      assertTrue(!!recentTabChip);
      assertEquals(tabInfo2, (recentTabChip as RecentTabChipElement).recentTab);
      assertEquals(3, composeboxElement.$.context.tabSuggestions.length);
    });

    test('hides recent tab chip when tab is in context', async () => {
      const tabInfo = {
        tabId: 1,
        title: 'Sample Tab',
        url: {url: 'https://example.com'},
        showInRecentTabChip: true,
        lastActive: {internalValue: 0n},
      };
      searchboxHandler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: [tabInfo]}));
      createComposeboxElement();
      const contextElement = composeboxElement.$.context;
      await microtasksFinished();
      await contextElement.updateComplete;

      // Add zps matches to ensure recent tab chip is visible.
      composeboxElement.$.input.value = '';
      composeboxElement.$.input.dispatchEvent(new Event('input'));
      await microtasksFinished();
      const matches = [
        createSearchMatch(),
        createSearchMatch({fillIntoEdit: 'hello world 2'}),
      ];
      searchboxCallbackRouterRemote.autocompleteResultChanged(
          createAutocompleteResult({
            matches: matches,
          }));
      await microtasksFinished();

      let recentTabChip = await getRecentTabChip();
      assertTrue(recentTabChip !== null);

      // Add the tab to the context.
      searchboxHandler.setResultFor(
          ADD_TAB_CONTEXT_FN,
          Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

      recentTabChip.shadowRoot!.querySelector<HTMLElement>(
                                   'cr-button')!.click();
      await searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
      await microtasksFinished();

      recentTabChip = await getRecentTabChip();

      assertTrue(recentTabChip === null);
    });

    test('setSearchContext sets input and queries autocomplete', async () => {
      loadTimeData.overrideValues({composeboxShowZps: true});
      composeboxElement = new ComposeboxElement();
      // TODO(crbug.com/460551908): Replace `ntpRealboxNextEnabled` with
      // whatever is used to delineate the Omnibox's composebox from the
      // NTP's.
      composeboxElement.ntpRealboxNextEnabled = true;
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
      composeboxElement.setSearchContext(context);
      await microtasksFinished();

      // Check that input and lastQueriedInput are set.
      assertEquals(composeboxElement.getText(), 'hello world');
      assertEquals((composeboxElement as any).lastQueriedInput_, 'hello world');
      // Autocomplete should be queried again.
      assertEquals(searchboxHandler.getCallCount('queryAutocomplete'), 1);
    });
  });
});
