// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {ContextUploadErrorType, ContextUploadStatus, InputType, ToolMode as ComposeboxToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle} from '../test_support.js';

import {ADD_FILE_CONTEXT_FN, ADD_TAB_CONTEXT_FN, addTab, areMatchesShowing, createComposeboxElement, FAKE_TOKEN_STRING, FAKE_TOKEN_STRING_2, generateZeroId, getInputForFileType, getMockFileChangeEventForType, getSubmitContainer, mockInputState, setupComposeboxTest, uploadFileAndVerify, waitForAddFileCallCount} from './test_support.js';

suite('NewTabPageComposeboxUploadTest', () => {
  const testProxy = setupComposeboxTest();

  test('upload image', async () => {
    createComposeboxElement(testProxy);
    // Submit button is disabled without any input.
    assertStyle(getSubmitContainer(testProxy), 'cursor', 'not-allowed');
    await uploadFileAndVerify(
        testProxy, FAKE_TOKEN_STRING,
        new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        null,
    );
    await testProxy.element.updateComplete;
    await microtasksFinished();

    assertStyle(getSubmitContainer(testProxy), 'cursor', 'pointer');
  });

  test(
      'upload image works when config is set to wildcard image/*', async () => {
        loadTimeData.overrideValues({
          'composeboxImageFileTypes': 'image/*',
        });
        createComposeboxElement(testProxy);
        const token = {low: BigInt(1), high: BigInt(2)};
        const file = new File(['foo'], 'foo.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(testProxy, token, file);
      });

  test('uploading/deleting pdf file queries zps', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowSubmit: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Autocomplete queried once when composebox is opened.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
    const id = generateZeroId();
    await uploadFileAndVerify(
        testProxy, id, new File(['foo'], 'foo.pdf', {type: 'application/pdf'}));
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, ContextUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    // Autocomplete should be stopped (with matches cleared) and then
    // queried again when a file is uploaded.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 1);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 2);

    // The suggest request should be triggered before the file has finished
    // uploading.
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, ContextUploadStatus.kUploadSuccessful, null);

    // Delete the uploaded file.
    const deletedId = testProxy.element.$.carousel.files[0]!.uuid;
    testProxy.element.$.carousel.dispatchEvent(new CustomEvent('delete-file', {
      detail: {
        uuid: deletedId,
      },
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();

    // Deleting a file should also stop autocomplete (and clear matches) and
    // then query autocomplete again for unimodal zps results.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 2);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 3);
  });

  test('uploading image file without flag does nothing', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowImageSuggest: false});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Autocomplete queried once when composebox is opened.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
    const id = generateZeroId();
    await uploadFileAndVerify(
        testProxy, id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, ContextUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    // Autocomplete should not be queried again since the uploaded file is an
    // image and the image suggest flag is disabled.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
  });

  test('uploading image file with image suggest flag queries zps', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowImageSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Autocomplete queried once when composebox is opened.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
    const id = generateZeroId();
    await uploadFileAndVerify(
        testProxy, id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, ContextUploadStatus.kProcessingSuggestSignalsReady, null);
    await microtasksFinished();

    // Autocomplete should be stopped (with matches cleared) and then
    // queried again when a file is uploaded.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 1);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 2);
  });

  [new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
   new File(['foo'], 'foo.pdf', {
     type: 'application/pdf',
   })].forEach((file) => {
    test(
        `announce file upload started and completed: ${file.type}`,
        async () => {
          createComposeboxElement(testProxy);

          let announcementCount = 0;
          const updateAnnouncementCount = () => {
            announcementCount += 1;
          };
          document.body.addEventListener(
              'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
          let announcementPromise =
              eventToPromise('cr-a11y-announcer-messages-sent', document.body);

          const id = generateZeroId();
          await uploadFileAndVerify(testProxy, id, file);

          let announcement = await announcementPromise;
          assertEquals(announcementCount, 1);
          assertTrue(!!announcement);
          assertEquals(announcement.detail.messages.length, 1);

          testProxy.searchboxCallbackRouterRemote
              .onContextualInputStatusChanged(
                  id, ContextUploadStatus.kUploadSuccessful, null);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

          announcementPromise =
              eventToPromise('cr-a11y-announcer-messages-sent', document.body);
          announcement = await announcementPromise;
          assertEquals(announcementCount, 2);
          assertTrue(!!announcement);
          assertEquals(announcement.detail.messages.length, 1);

          // Cleanup event listener.
          document.body.removeEventListener(
              'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
          assertEquals(
              1,
              testProxy.metrics.count(
                  'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage',
                  0));
        });
  });

  test('upload empty file fails', async () => {
    createComposeboxElement(testProxy);
    const file = new File([''], 'foo.jpg', {type: 'image/jpeg'});

    // Act.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    const input: HTMLInputElement =
        getInputForFileType(testProxy.element, file.type);
    input.files = dataTransfer.files;
    input.dispatchEvent(
        getMockFileChangeEventForType(testProxy.element, file.type));
    await microtasksFinished();

    // Assert no files uploaded or rendered on the carousel
    assertEquals(
        testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 0);
    assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));
    assertEquals(
        1,
        testProxy.metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 2));
  });

  test('upload large file fails', async () => {
    const sampleFileMaxSize = 10;
    loadTimeData.overrideValues({'composeboxFileMaxSize': sampleFileMaxSize});
    createComposeboxElement(testProxy);
    const blob = new Blob(
        [new Uint8Array(sampleFileMaxSize + 1)],
        {type: 'application/octet-stream'});
    const file = new File([blob], 'foo.jpg', {type: 'image/jpeg'});

    // Act.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    const input: HTMLInputElement =
        getInputForFileType(testProxy.element, file.type);
    input.files = dataTransfer.files;
    input.dispatchEvent(
        getMockFileChangeEventForType(testProxy.element, file.type));
    await microtasksFinished();

    // Assert no files uploaded or rendered on the carousel
    assertEquals(
        testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 0);
    assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));
    assertEquals(
        1,
        testProxy.metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 3));
  });

  [[
    ContextUploadStatus.kValidationFailed,
    ContextUploadErrorType.kImageProcessingError,
  ],
   [
     ContextUploadStatus.kUploadFailed,
     null,
   ],
   [
     ContextUploadStatus.kUploadExpired,
     null,
   ],
  ].forEach(([fileUploadStatus, fileUploadErrorType, ..._]) => {
    test(
        `Image upload is removed on failed upload status ${fileUploadStatus}`,
        async () => {
          createComposeboxElement(testProxy);
          const id = generateZeroId();
          const file = new File(['foo'], 'foo.jpg', {type: 'image/jpeg'});
          await uploadFileAndVerify(testProxy, id, file);

          testProxy.searchboxCallbackRouterRemote
              .onContextualInputStatusChanged(
                  id, fileUploadStatus as ContextUploadStatus,
                  fileUploadErrorType as ContextUploadErrorType | null);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

          // Assert no files in the carousel.
          assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

          if (fileUploadErrorType !== null) {
            assertEquals(
                loadTimeData.getString('composeFileTypesAllowedError'),
                testProxy.element.$.errorScrim.errorMessage);
          }
        });
  });

  test('upload pdf', async () => {
    createComposeboxElement(testProxy);
    testProxy.searchboxHandler.setPromiseResolveFor(
        ADD_FILE_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    // Assert no files.
    assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

    // Arrange.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    testProxy.element.$.fileInputs.$.fileInput.files = dataTransfer.files;
    testProxy.element.$.fileInputs.$.fileInput.dispatchEvent(
        new Event('change'));

    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert one pdf file.
    const files = testProxy.element.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'application/pdf');
    assertEquals(files[0]!.name, 'foo.pdf');
    assertFalse(!!files[0]!.objectUrl);

    assertEquals(
        testProxy.searchboxHandler.getCallCount('notifySessionStarted'), 1);

    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    // Assert file is uploaded.
    assertEquals(
        testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN), 1);
    const [[fileInfo, fileData]] =
        testProxy.searchboxHandler.getArgs(ADD_FILE_CONTEXT_FN);
    assertEquals(fileInfo.fileName, 'foo.pdf');
    assertDeepEquals(fileData.bytes, fileArray);
    // Assert context added method was context menu.
    const CONTEXT_ADDED_NTP =
        'ContextualSearch.ContextAdded.ContextAddedMethod.NewTabPage';
    assertEquals(1, testProxy.metrics.count(CONTEXT_ADDED_NTP));
    assertEquals(
        1,
        testProxy.metrics.count(
            CONTEXT_ADDED_NTP,
            /* CONTEXT_MENU */ 0));
  });

  test('delete file', async () => {
    loadTimeData.overrideValues({composeboxFileMaxCount: 5});
    createComposeboxElement(testProxy);
    let i = 0;
    testProxy.searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
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
      value: testProxy.element.$.fileInputs.$.fileInput,
    });

    testProxy.element.$.fileInputs.$.fileInput.files = dataTransfer.files;
    testProxy.element.$.fileInputs.$.fileInput.dispatchEvent(mockFileChange);

    await waitForAddFileCallCount(testProxy.searchboxHandler, 2);
    await testProxy.element.updateComplete;
    await microtasksFinished();

    // Assert two files are present initially.
    assertEquals(testProxy.element.$.carousel.files.length, 2);

    // Act.
    const deletedId = testProxy.element.$.carousel.files[0]!.uuid;
    testProxy.element.$.carousel.dispatchEvent(new CustomEvent('delete-file', {
      detail: {
        uuid: deletedId,
      },
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();

    // Assert.
    assertEquals(testProxy.element.$.carousel.files.length, 1);
    assertEquals(testProxy.searchboxHandler.getCallCount('deleteContext'), 1);
    const [idArg, fromChip] =
        testProxy.searchboxHandler.getArgs('deleteContext')[0];
    assertEquals(idArg, deletedId);
    assertFalse(fromChip);
  });

  test('image upload button clicks file input', () => {
    loadTimeData.overrideValues({
      'composeboxShowContextMenu': true,
    });
    createComposeboxElement(testProxy);
    let clickCalled = false;
    testProxy.element.$.fileInputs.$.imageInput.click = () => {
      clickCalled = true;
    };
    const contextEntrypoint = $$(testProxy.element, '#contextEntrypoint');
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
    createComposeboxElement(testProxy);
    let clickCalled = false;
    testProxy.element.$.fileInputs.$.fileInput.click = () => {
      clickCalled = true;
    };
    const contextEntrypoint = $$(testProxy.element, '#contextEntrypoint');
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
        createComposeboxElement(testProxy);
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
            testInputState);
        await microtasksFinished();

        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_FILE_CONTEXT_FN, {token: {low: BigInt(1), high: BigInt(2)}});

        // Upload a PDF file.
        const pdfFile = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(pdfFile);
        testProxy.element.$.fileInputs.$.fileInput.files = dataTransfer.files;
        testProxy.element.$.fileInputs.$.fileInput.dispatchEvent(
            new Event('change'));

        await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
        await microtasksFinished();
        assertFalse(testProxy.element['uploadButtonDisabled_']);

        // Delete the file. `uploadButtonDisabled` should be false.
        const deletedId = testProxy.element.$.carousel.files[0]!.uuid;
        testProxy.element.$.carousel.dispatchEvent(new CustomEvent(
            'delete-file',
            {detail: {uuid: deletedId}, bubbles: true, composed: true}));
        await microtasksFinished();
        assertFalse(testProxy.element['uploadButtonDisabled_']);
        testProxy.searchboxHandler.resetResolver(ADD_FILE_CONTEXT_FN);
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_FILE_CONTEXT_FN, {token: {low: BigInt(3), high: BigInt(4)}});

        // Upload an image file. `uploadButtonDisabled` should be false.
        const imageFile = new File(['foo'], 'foo.png', {type: 'image/png'});
        const dataTransfer2 = new DataTransfer();
        dataTransfer2.items.add(imageFile);

        const imageInput = testProxy.element.$.fileInputs.$.imageInput;
        imageInput.files = dataTransfer2.files;
        imageInput.dispatchEvent(new Event('change'));

        await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
        await microtasksFinished();
        assertFalse(testProxy.element['uploadButtonDisabled_']);

        // Enter create image mode.
        testProxy.element['activeToolMode_'] = ComposeboxToolMode.kImageGen;
        await testProxy.element.updateComplete;
        assertFalse(testProxy.element['uploadButtonDisabled_']);

        // Exit create image mode. `uploadButtonDisabled` should be false.
        testProxy.element['activeToolMode_'] = ComposeboxToolMode.kUnspecified;
        await testProxy.element.updateComplete;
        assertFalse(testProxy.element['uploadButtonDisabled_']);
      });

  test('pasting valid files calls addFileContext', async () => {
    // Arrange.
    loadTimeData.overrideValues({'composeboxFileMaxCount': 5});
    createComposeboxElement(testProxy);
    testProxy.searchboxHandler.setPromiseResolveFor(
        ADD_FILE_CONTEXT_FN, {token: {low: BigInt(1), high: BigInt(2)}});

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
    testProxy.element.$.input.dispatchEvent(pasteEvent);

    // Assert.
    // Check that addFileContext (ADD_FILE_CONTEXT_FN) was called twice.
    await waitForAddFileCallCount(testProxy.searchboxHandler, 2);
    const [[fileInfo1], [fileInfo2]] =
        testProxy.searchboxHandler.getArgs(ADD_FILE_CONTEXT_FN);
    assertEquals('foo.png', fileInfo1.fileName);
    assertEquals('foo.pdf', fileInfo2.fileName);

    // Check that the default paste event was prevented.
    assertTrue(pasteEvent.defaultPrevented);
    const CONTEXT_ADDED_NTP =
        'ContextualSearch.ContextAdded.ContextAddedMethod.NewTabPage';
    assertEquals(1, testProxy.metrics.count(CONTEXT_ADDED_NTP));
    assertEquals(
        1,
        testProxy.metrics.count(
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
    createComposeboxElement(testProxy);
    testProxy.searchboxCallbackRouterRemote.onInputStateChanged(testInputState);
    await microtasksFinished();

    testProxy.searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
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
    testProxy.element.$.input.dispatchEvent(pasteEvent);
    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert.
    // Check that only one files were added.
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // Check that the "too many files" metric was recorded (Enum value 1).
    assertEquals(
        1,
        testProxy.metrics.count(
            'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage', 1));

    // Check that the paste event was prevented.
    assertTrue(pasteEvent.defaultPrevented);

    // Check whether the right error would show up.
    assertEquals(
        loadTimeData.getString('maxImagesReachedError'),
        testProxy.element.$.errorScrim.errorMessage);
  });

  test('pasting unsupported files fires validation error', async () => {
    // Arrange.
    createComposeboxElement(testProxy);
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
    testProxy.element.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    // Assert.
    // Check that the correct error event was fired.
    assertEquals(
        loadTimeData.getString('composeFileTypesAllowedError'),
        testProxy.element.$.errorScrim.errorMessage);

    // Check that no files were added.
    assertEquals(
        0, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // Check that the paste event was prevented.
    assertTrue(pasteEvent.defaultPrevented);
  });

  test(
      'pasting only text does not call addFiles or prevent default',
      async () => {
        // Arrange.
        createComposeboxElement(testProxy);
        const dataTransfer = new DataTransfer();
        dataTransfer.setData('text/plain', 'hello');
        const pasteEvent = new ClipboardEvent('paste', {
          clipboardData: dataTransfer,
          bubbles: true,
          cancelable: true,
          composed: true,
        });

        // Act.
        testProxy.element.$.input.dispatchEvent(pasteEvent);
        await microtasksFinished();

        // Assert.
        // Check that no files were added.
        assertEquals(
            0, testProxy.searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

        // Check the paste event was not prevented (onPaste_ returns early).
        assertFalse(pasteEvent.defaultPrevented);
      });

  test('pasting mixed files is processed correctly ', async () => {
    // Arrange.
    createComposeboxElement(testProxy);
    let i = 0;
    testProxy.searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
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
    testProxy.element.$.input.dispatchEvent(pasteEvent);

    // Wait for both files to be processed (addFileContext called twice).
    await waitForAddFileCallCount(testProxy.searchboxHandler, 2);
    await microtasksFinished();

    // Assert.
    // Check if the Carousel received 2 files.
    const files = testProxy.element.$.carousel.files;
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

  test(
      'uploading 6 valid files when limit is 5 uploads 5 and shows error',
      async () => {
        // Arrange.
        const testInputState = {
          ...mockInputState,
          maxTotalInputs: 5,
        };
        createComposeboxElement(testProxy);
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
            testInputState);
        await microtasksFinished();

        let i = 0;
        testProxy.searchboxHandler.setResultMapperFor(
            ADD_FILE_CONTEXT_FN, () => {
              i++;
              return Promise.resolve({low: BigInt(i), high: BigInt(0)});
            });

        const validFiles = Array.from(
            {length: 6},
            (_, i) => new File(['foo'], `good${i}.png`, {type: 'image/png'}));

        const dataTransfer = new DataTransfer();
        validFiles.forEach(file => dataTransfer.items.add(file));

        const pasteEvent = new ClipboardEvent('paste', {
          clipboardData: dataTransfer,
          bubbles: true,
          cancelable: true,
          composed: true,
        });

        // Act.
        testProxy.element.$.input.dispatchEvent(pasteEvent);

        await waitForAddFileCallCount(testProxy.searchboxHandler, 5);
        await microtasksFinished();

        // Assert.
        assertEquals(5, testProxy.element.$.carousel.files.length);

        assertEquals(
            loadTimeData.getString('maxImagesReachedError'),
            testProxy.element.$.errorScrim.errorMessage);

        assertEquals(
            1,
            testProxy.metrics.count(
                'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage',
                1));
      });

  test(
      'upload mixed files over limit prioritizes max files error and uploads valid ones',
      async () => {
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
        createComposeboxElement(testProxy);
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
            testInputState);
        await microtasksFinished();

        let i = 0;
        testProxy.searchboxHandler.setResultMapperFor(
            ADD_FILE_CONTEXT_FN, () => {
              i++;
              return Promise.resolve(
                  {token: {low: BigInt(i), high: BigInt(0)}});
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
        testProxy.element.$.input.dispatchEvent(pasteEvent);

        await waitForAddFileCallCount(testProxy.searchboxHandler, 3);
        await microtasksFinished();

        // Assert.
        assertEquals(3, testProxy.element.$.carousel.files.length);

        assertEquals(
            loadTimeData.getString('maxFilesReachedError'),
            testProxy.element.$.errorScrim.errorMessage);

        assertEquals(
            1,
            testProxy.metrics.count(
                'ContextualSearch.File.WebUI.UploadAttemptFailure.NewTabPage',
                1));
      });

  test(
      'uploading valid heif and invalid svg adds valid file and shows error',
      async () => {
        createComposeboxElement(testProxy);


        let i = 0;
        testProxy.searchboxHandler.setResultMapperFor(
            ADD_FILE_CONTEXT_FN, () => {
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

        testProxy.element.$.input.dispatchEvent(pasteEvent);

        await waitForAddFileCallCount(testProxy.searchboxHandler, 1);
        await microtasksFinished();

        assertEquals(1, testProxy.element.$.carousel.files.length);
        assertEquals(

            'image.png', testProxy.element.$.carousel.files[0]!.name);

        assertEquals(
            loadTimeData.getString('composeFileTypesAllowedError'),
            testProxy.element.$.errorScrim.errorMessage);
      });

  test('correctly sets create image mode', async () => {
    loadTimeData.overrideValues({
      composeboxShowZps: true,
      composeboxShowTypedSuggest: false,
      'composeboxFileMaxCount': 1,
    });
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Enter create image mode.
    const contextEntrypoint = $$(testProxy.element, '#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
      detail: {toolMode: ComposeboxToolMode.kImageGen},
    }));
    await microtasksFinished();
    assertEquals(
        testProxy.searchboxHandler.getCallCount('setActiveToolMode'), 1);
    assertEquals(
        ComposeboxToolMode.kImageGen,
        testProxy.searchboxHandler.getArgs('setActiveToolMode')[0]);
  });

  test('composebox does not open match when only file present', async () => {
    createComposeboxElement(testProxy);

    assertEquals(testProxy.searchboxHandler.getCallCount('submitQuery'), 0);
    await uploadFileAndVerify(
        testProxy, FAKE_TOKEN_STRING,
        new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );
    await microtasksFinished();

    getSubmitContainer(testProxy).click();
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('submitQuery'), 1,
        'submitQuery count should be 1');
    assertEquals(
        testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'), 0,
        'openAutocompleteMatch count should be 0');
  });

  test('composebox does not show when image is present', async () => {
    loadTimeData.overrideValues({
      composeboxShowZps: true,
      composeboxShowTypedSuggest: true,
      composeboxShowImageSuggest: false,
    });
    createComposeboxElement(testProxy);
    // Autocomplete queried once when composebox is created.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);

    const matches = [createSearchMatchForTesting()];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches,
        }));
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    // Upload an image.
    const id = generateZeroId();
    await uploadFileAndVerify(
        testProxy, id, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));

    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        id, ContextUploadStatus.kProcessingSuggestSignalsReady, null);

    // Matches should not show when image is present.
    assertFalse(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    // Query autocomplete with image present to get verbatim match.
    testProxy.element.$.input.value = 'T';
    testProxy.element.$.input.dispatchEvent(new Event('input'));
    await microtasksFinished();
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 2);
  });

  test('add file context fails', async () => {
    loadTimeData.overrideValues({composeboxShowPdfUpload: true});
    createComposeboxElement(testProxy);
    // Set the promise to reject to simulate a failure.
    testProxy.searchboxHandler.setResultMapperFor(ADD_FILE_CONTEXT_FN, () => {
      return Promise.reject(ContextUploadErrorType.kBrowserProcessingError);
    });

    // Assert no files.
    assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

    // Act.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    testProxy.element.$.fileInputs.$.fileInput.files = dataTransfer.files;
    testProxy.element.$.fileInputs.$.fileInput.dispatchEvent(
        new Event('change'));

    await testProxy.searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    await microtasksFinished();

    // Assert no files in carousel.
    assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

    assertEquals(
        loadTimeData.getString('composeboxFileUploadFailed'),
        testProxy.element.$.errorScrim.errorMessage);
  });

  test('when flag enabled, adds tab context of ghost file', async () => {
    createComposeboxElement(testProxy);
    testProxy.element.shouldShowGhostFiles = true;

    await addTab(testProxy);

    await testProxy.element.updateComplete;
    await microtasksFinished();

    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 1,
        'Tab should be added');

    const bad_token = FAKE_TOKEN_STRING_2;
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        bad_token,
        ContextUploadStatus.kUploadSuccessful,
        null,
    );
    await testProxy.element.updateComplete;
    await microtasksFinished();
    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 2,
        'Ghost file should be added');
  });

  test('does not add tab context of ghost file', async () => {
    createComposeboxElement(testProxy);
    testProxy.element.shouldShowGhostFiles = false;

    await addTab(testProxy);
    await testProxy.element.updateComplete;
    await microtasksFinished();


    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 1,
        'Tab should be added');
    const bad_token = FAKE_TOKEN_STRING_2;
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        bad_token,
        ContextUploadStatus.kUploadSuccessful,
        null,
    );
    await testProxy.element.updateComplete;
    await microtasksFinished();
    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 1,
        'Ghost file should not be added');
  });

  test(
      'regular auto chip de-duplication logic resets when clearing' +
          ' all (entering new thread, mode, etc.)',
      async () => {
        createComposeboxElement(testProxy);

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'Should be 0 starting test');
        const tab = {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        } as any as TabInfo;

        testProxy.searchboxHandler.resetResolver(ADD_TAB_CONTEXT_FN);
        // Initiate regular de-duplication logic by properly mocking
        // `addTabContext` return value.
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING);

        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab);

        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await testProxy.element.updateComplete;
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 1,
            'Attached files should be 1 after adding first tab.');

        testProxy.element.clearAllInputs(
            /*querySubmitted*/ false,
            /*shouldBlockAutoSuggestedTabs=*/ false);

        await testProxy.element.updateComplete;
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'Should be 0 after clearing all.');

        testProxy.searchboxHandler.resetResolver(ADD_TAB_CONTEXT_FN);
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING);

        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 1,
            'Attached files should be 1 after adding a second auto ' +
                'chip, and having cleared the first one.');
      });

  test(
      'auto chip de-duplication logic does not rely on callback states',
      async () => {
        createComposeboxElement(testProxy);

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'Attached files should be 0 at start.');

        const tab = {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        } as any as TabInfo;

        // Do not mock `addTabContext` return value so callback does not
        // update current held auto chip context properly.
        // Relying purely on callback auto chip context states is incorrect
        // and will result in depending on this bad mock return value.
        testProxy.searchboxHandler.resetResolver(ADD_TAB_CONTEXT_FN);
        testProxy.searchboxHandler.setPromiseResolveFor(ADD_TAB_CONTEXT_FN, '');

        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await testProxy.element.updateComplete;
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'Attached files should be 0 after failed callback' +
                'does not return for an auto chip.');

        testProxy.searchboxHandler.resetResolver(ADD_TAB_CONTEXT_FN);
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING);

        // Should not duplicate auto chip, even with the same tab
        // being called (same tab id, url) since pending auto chip context
        // is updated synchronously from the last
        // `updateAutoSuggestedTabContext` call, and does not rely on the
        // callback result.

        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab);

        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await testProxy.element.updateComplete;
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'Attached files should still be 0 since the first' +
                'callback corrupted, but the same auto chip' +
                'context is added again.');

        const tab2 = {
          tabId: 2,
          title: 'Tab 2',
          url: 'https://example2.com',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(4)},
        } as any as TabInfo;

        testProxy.searchboxHandler.resetResolver(ADD_TAB_CONTEXT_FN);
        // Proper mock for tab2.
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING_2);

        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab2);

        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await testProxy.element.updateComplete;
        await microtasksFinished();

        // New auto chip added since is different from tab 1.
        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 1,
            'Attached files should be 1 after adding a second auto ' +
                'chip, and having the first one be corrupted.');
      });

  test(
      'pending auto chip de-duplication logic resets when clearing' +
          ' all (entering new thread, mode, etc.)',
      async () => {
        createComposeboxElement(testProxy);

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'Should be 0 starting test');
        const tab = {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        } as any as TabInfo;

        testProxy.searchboxHandler.resetResolver(ADD_TAB_CONTEXT_FN);
        // Do not mock `addTabContext` return value so callback does not
        // update current held auto chip context properly. This simulates
        // relying on pending auto chip context state.
        testProxy.searchboxHandler.setPromiseResolveFor(ADD_TAB_CONTEXT_FN, '');

        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await testProxy.element.updateComplete;
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'First tab should not be added since callback fails');

        testProxy.element.clearAllInputs(
            /*querySubmitted*/ false,
            /*shouldBlockAutoSuggestedTabs=*/ false);

        await testProxy.element.updateComplete;
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 0,
            'Should be 0 after clearing all.');

        testProxy.searchboxHandler.resetResolver(ADD_TAB_CONTEXT_FN);
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING);

        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

        await testProxy.element.updateComplete;
        await microtasksFinished();

        assertEquals(
            testProxy.element.getNumOfFilesForTesting(), 1,
            'Same tab should be added back after clearing all.');
      });
});
