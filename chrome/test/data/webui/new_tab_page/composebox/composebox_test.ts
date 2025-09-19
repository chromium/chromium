// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxElement, ComposeboxProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {PageRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {FileUploadErrorType, FileUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
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
  let callbackRouterRemote: PageRemote;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    callbackRouterRemote = ComposeboxProxyImpl.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
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
        const currentCount = handler.getCallCount(ADD_FILE_CONTEXT_FN);
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
    return fileType === 'application/pdf' ? composeboxElement.$.fileInput :
                                            composeboxElement.$.imageInput;
  }

  function getMockFileChangeEventForType(fileType: string): Event {
    if (fileType === 'application/pdf') {
      return new Event('change');
    }

    const mockFileChange = new Event('change', {bubbles: true});
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: composeboxElement.$.imageInput,
    });
    return mockFileChange;
  }

  function createAutocompleteMatch(): AutocompleteMatch {
    return {
      a11yLabel: {data: []},
      actions: [],
      allowedToBeDefaultMatch: false,
      isSearchType: false,
      isEnterpriseSearchAggregatorPeopleType: false,
      swapContentsAndDescription: false,
      supportsDeletion: false,
      suggestionGroupId: -1,  // Indicates a missing suggestion group Id.
      contents: {data: []},
      contentsClass: [{offset: 0, style: 0}],
      description: {data: []},
      descriptionClass: [{offset: 0, style: 0}],
      destinationUrl: {url: ''},
      inlineAutocompletion: {data: []},
      fillIntoEdit: {data: []},
      iconPath: '',
      iconUrl: {url: ''},
      imageDominantColor: '',
      imageUrl: '',
      isNoncannedAimSuggestion: false,
      removeButtonA11yLabel: {data: []},
      type: '',
      isRichSuggestion: false,
      isWeatherAnswerSuggestion: null,
      answer: null,
      tailSuggestCommonPrefix: null,
    };
  }

  function createAutocompleteResult(
      modifiers: Partial<AutocompleteResult> = {}): AutocompleteResult {
    const base: AutocompleteResult = {
      input: stringToMojoString16(''),
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
          contents: stringToMojoString16('hello world'),
          destinationUrl: {url: 'https://www.google.com/search?q=hello+world'},
          fillIntoEdit: stringToMojoString16('hello world'),
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
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    handler.setResultFor(ADD_FILE_CONTEXT_FN, Promise.resolve({token: token}));

    // Act.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);

    const input: HTMLInputElement = getInputForFileType(file.type);
    input.files = dataTransfer.files;
    input.dispatchEvent(getMockFileChangeEventForType(file.type));

    await handler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    assertEquals(handler.getCallCount('notifySessionStarted'), 1);
    await verifyFileUpload(file);
  }

  async function verifyFileUpload(file: File) {
    // Assert one file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);

    assertEquals(files[0]!.type, file.type);
    assertEquals(files[0]!.name, file.name);

    // Assert file is uploaded.
    assertEquals(handler.getCallCount(ADD_FILE_CONTEXT_FN), 1);

    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    const [[fileInfo, fileData]] = handler.getArgs(ADD_FILE_CONTEXT_FN);
    assertEquals(fileInfo.fileName, file.name);
    assertDeepEquals(fileData.bytes, fileArray);
  }

  test('clear functionality', async () => {
    createComposeboxElement();
    handler.setResultFor(
        ADD_FILE_CONTEXT_FN,
        Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

    // Check submit button disabled.
    assertStyle(composeboxElement.$.submitIcon, 'cursor', 'default');
    // Add input.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(new Event('change'));

    await handler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Check submit button enabled and file uploaded.
    assertStyle(composeboxElement.$.submitIcon, 'cursor', 'pointer');
    assertEquals(composeboxElement.$.carousel.files.length, 1);

    // Clear input.
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await microtasksFinished();

    // Assert
    assertEquals(handler.getCallCount('clearFiles'), 1);

    // Check submit button disabled and files empty.
    assertStyle(composeboxElement.$.submitIcon, 'cursor', 'default');
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Close composebox.
    const whenCloseComposebox =
        eventToPromise('close-composebox', composeboxElement);
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await whenCloseComposebox;
  });

  test('upload image', async () => {
    createComposeboxElement();
    assertStyle(composeboxElement.$.submitIcon, 'cursor', 'default');
    const token = {low: BigInt(1), high: BigInt(2)};
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    assertStyle(composeboxElement.$.submitIcon, 'cursor', 'pointer');
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

              callbackRouterRemote.onContextualInputStatusChanged(
                  id, FileUploadStatus.kUploadSuccessful, null);
              await callbackRouterRemote.$.flushForTesting();

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
                      'NewTabPage.Composebox.File.WebUI.UploadAttemptFailure',
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
    assertEquals(handler.getCallCount(ADD_FILE_CONTEXT_FN), 0);
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 0);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Composebox.File.WebUI.UploadAttemptFailure', 2));
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
    assertEquals(handler.getCallCount(ADD_FILE_CONTEXT_FN), 0);
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 0);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Composebox.File.WebUI.UploadAttemptFailure', 3));
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

          callbackRouterRemote.onContextualInputStatusChanged(
              id, fileUploadStatus as FileUploadStatus,
              fileUploadErrorType as FileUploadErrorType | null);
          await callbackRouterRemote.$.flushForTesting();

          // Assert no files in the carousel.
          const files = composeboxElement.$.carousel.files;
          assertEquals(files.length, 0);
        });
  });

  test('upload pdf', async () => {
    createComposeboxElement();
    handler.setResultFor(
        ADD_FILE_CONTEXT_FN,
        Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

    // Assert no files.
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Arrange.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(new Event('change'));

    await handler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert one pdf file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'application/pdf');
    assertEquals(files[0]!.name, 'foo.pdf');
    assertFalse(!!files[0]!.objectUrl);

    assertEquals(handler.getCallCount('notifySessionStarted'), 1);

    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    // Assert file is uploaded.
    assertEquals(handler.getCallCount(ADD_FILE_CONTEXT_FN), 1);
    const [[fileInfo, fileData]] = handler.getArgs(ADD_FILE_CONTEXT_FN);
    assertEquals(fileInfo.fileName, 'foo.pdf');
    assertDeepEquals(fileData.bytes, fileArray);
  });

  test('delete file', async () => {
    createComposeboxElement();
    let i = 0;
    handler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
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
      value: composeboxElement.$.fileInput,
    });

    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(mockFileChange);

    await waitForAddFileCallCount(2);
    await composeboxElement.updateComplete;
    await microtasksFinished();

    // Assert two files are present initially.
    assertEquals(composeboxElement.$.carousel.files.length, 2);

    // Act.
    const deletedId = composeboxElement.$.carousel.files[0]!.uuid;
    composeboxElement.$.carousel.dispatchEvent(new CustomEvent('delete-file', {
      detail: {
        uuid: deletedId,
      },
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();

    // Assert.
    assertEquals(composeboxElement.$.carousel.files.length, 1);
    assertEquals(handler.getCallCount('deleteContext'), 1);
    const [idArg] = handler.getArgs('deleteContext');
    assertEquals(idArg, deletedId);
  });

  test('NotifySessionStarted called on composebox created', () => {
    // Assert call has not occurred.
    assertEquals(handler.getCallCount('notifySessionStarted'), 0);

    createComposeboxElement();

    // Assert call occurs.
    assertEquals(handler.getCallCount('notifySessionStarted'), 1);
  });

  test('image upload button clicks file input', async () => {
    loadTimeData.overrideValues({
      'composeboxShowContextMenu': false,
    });
    createComposeboxElement();
    const imageUploadEventPromise =
        eventToPromise('click', composeboxElement.$.imageInput);
    composeboxElement.$.imageUploadButton.click();

    // Assert.
    await imageUploadEventPromise;
  });

  test('file upload button clicks file input', async () => {
    loadTimeData.overrideValues({
      'composeboxShowPdfUpload': true,
      'composeboxShowContextMenu': false,
    });
    createComposeboxElement();
    const fileUploadClickEventPromise =
        eventToPromise('click', composeboxElement.$.fileInput);
    composeboxElement.$.fileUploadButton.click();

    // Assert.
    await fileUploadClickEventPromise;
  });

  test('disabling file upload does not show fileUploadButton', async () => {
    loadTimeData.overrideValues({'composeboxShowPdfUpload': false});
    createComposeboxElement();
    await composeboxElement.updateComplete;

    // Assert
    assertFalse(
        !!composeboxElement.shadowRoot.querySelector('#fileUploadButton'));
  });

  test('file upload buttons disabled when max files uploaded', async () => {
    loadTimeData.overrideValues({'composeboxFileMaxCount': 1});
    loadTimeData.overrideValues({'composeboxShowPdfUpload': true});
    createComposeboxElement();
    handler.setResultFor(
        ADD_FILE_CONTEXT_FN,
        Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

    // File upload buttons are not disabled when there are no files.
    assertFalse(composeboxElement.$.fileUploadButton.disabled);
    assertFalse(composeboxElement.$.imageUploadButton.disabled);

    // Arrange.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(new Event('change'));

    await handler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert.
    assertTrue(composeboxElement.$.fileUploadButton.disabled);
    assertTrue(composeboxElement.$.imageUploadButton.disabled);
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
    await whenCloseComposebox;
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
          input: stringToMojoString16('test'),
          matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    composeboxElement.$.submitIcon.click();
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 1);
  });

  test('empty input does not lead to submission', async () => {
    createComposeboxElement();

    // Arrange.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();
    composeboxElement.$.submitIcon.click();
    await microtasksFinished();

    // Assert call does not occur.
    assertEquals(handler.getCallCount('submitQuery'), 0);
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);
  });

  test('submit button is disabled', async () => {
    // Arrange.
    composeboxElement.$.input.value = ' ';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();

    // Assert.
    assertTrue(composeboxElement.$.submitIcon.hasAttribute('disabled'));
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
          input: stringToMojoString16('test'),
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
      createSearchMatch({fillIntoEdit: stringToMojoString16('hello world 2')}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          matches: matches,
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown.hidden);
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
      createSearchMatch({fillIntoEdit: stringToMojoString16('hello world 2')}),
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

  test('arrow up/down moves selection / focus', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    // Add zps input.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    const matches = [
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: stringToMojoString16('hello world 2')}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          matches: matches,
        }));

    assertTrue(await areMatchesShowing());

    const matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'ntp-composebox-match');
    assertEquals(2, matchEls.length);

    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });

    composeboxElement.dispatchEvent(arrowDownEvent);
    await microtasksFinished();
    assertFalse(arrowDownEvent.defaultPrevented);

    // First match is not selected as focus is in input
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals('', composeboxElement.$.input.value);

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

  test('smart compose response added', async () => {
    createComposeboxElement();
    await microtasksFinished();

    // Add input.
    composeboxElement.$.input.value = 'smart ';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          input: stringToMojoString16('smart '),
          matches: [],
          smartComposeInlineHint: stringToMojoString16('compose'),
        }));
    await microtasksFinished();

    assertEquals('compose', composeboxElement.getSmartComposeForTesting());
  });

  test('tab adds smart compose to input', async () => {
    createComposeboxElement();
    await microtasksFinished();

    // Add input.
    composeboxElement.$.input.value = 'smart ';
    composeboxElement.$.input.dispatchEvent(new Event('input'));

    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          input: stringToMojoString16('smart '),
          matches: [],
          smartComposeInlineHint: stringToMojoString16('compose'),
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
  });

  test('composebox does not open match when only file present', async () => {
    createComposeboxElement();

    assertEquals(handler.getCallCount('submitQuery'), 0);
    const token = {low: BigInt(1), high: BigInt(2)};
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    composeboxElement.$.submitIcon.click();
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(handler.getCallCount('submitQuery'), 1);
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);
  });

  test('delete button removes match', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement();
    await microtasksFinished();

    const matches = [
      createSearchMatch(),
      createSearchMatch({fillIntoEdit: stringToMojoString16('hello world 2')}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResult({
          input: stringToMojoString16(''),
          matches,
          suggestionGroupsMap: {},
        }));

    assertTrue(await areMatchesShowing());

    const matchEls = composeboxElement.$.matches.shadowRoot.querySelectorAll(
        'ntp-composebox-match');
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
    assertEquals(0, handler.getCallCount('submitQuery'));
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
    assertEquals(0, handler.getCallCount('submitQuery'));
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

  suite('Context menu', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        composeboxShowContextMenu: true,
      });
    });

    test('context button replaces upload container', () => {
      createComposeboxElement();

      const uploadContainer = $$(composeboxElement, '#uploadContainer');
      assertFalse(!!uploadContainer);
      const contextMenuButton = $$(composeboxElement, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
    });

    test('add tab context', async () => {
      createComposeboxElement();
      handler.setResultFor(
          ADD_TAB_CONTEXT_FN,
          Promise.resolve({token: {low: BigInt(1), high: BigInt(2)}}));

      // Assert no files.
      assertEquals(composeboxElement.$.carousel.files.length, 0);

      const contextMenuButton = $$(composeboxElement, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
      const sampleTabTitle = 'Sample Tab';
      contextMenuButton.dispatchEvent(new CustomEvent('add-tab-context', {
        detail: {id: 1, title: sampleTabTitle},
        bubbles: true,
        composed: true,
      }));

      await handler.whenCalled(ADD_TAB_CONTEXT_FN);
      await microtasksFinished();
      const files = composeboxElement.$.carousel.files;
      assertEquals(files.length, 1);
      assertEquals(files[0]!.type, 'tab');
      assertEquals(files[0]!.name, sampleTabTitle);
    });
  });
});
