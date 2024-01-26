// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-feedback/file_attachment.js';
import 'chrome://os-feedback/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FileAttachmentElement} from 'chrome://os-feedback/file_attachment.js';
import {setFeedbackServiceProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {FeedbackAppPreSubmitAction} from 'chrome://os-feedback/os_feedback_ui.mojom-webui.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

const fakeImageUrl = 'chrome://os_feedback/app_icon_48.png';

const MAX_ATTACH_FILE_SIZE = 10 * 1024 * 1024;

suite('fileAttachmentTestSuite', () => {
  let page: FileAttachmentElement|null = null;

  let feedbackServiceProvider: FakeFeedbackServiceProvider|null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  });

  function initializePage() {
    page = document.createElement('file-attachment');
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  function getElement(selector: string): HTMLElement|null {
    return page!.shadowRoot!.querySelector(selector);
  }

  function getElementContent(selector: string): string {
    const element = getElement(selector);
    return element!.textContent!.trim();
  }

  function verifyRecordPreSubmitActionCallCount(
      callCounts: number, action: FeedbackAppPreSubmitAction) {
    assertEquals(
        callCounts,
        feedbackServiceProvider!.getRecordPreSubmitActionCallCount(action));
  }

  // Test the page is loaded with expected HTML elements.
  test('elementLoaded', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    assertEquals('Add file', getElementContent('#addFileLabel'));
    // Verify the i18n string is added.
    assertTrue(page!.i18nExists('addFileLabel'));
    // Verify the replace file label is in the page.
    assertEquals('Replace file', getElementContent('#replaceFileButton'));
    // The addFileContainer should be visible when no file is selected.
    assertTrue(isVisible(getElement('#addFileContainer')));
    // The replaceFileContainer should be invisible when no file is selected.
    assertFalse(isVisible(getElement('#replaceFileContainer')));

    assertTrue(!!getElement('#fileTooBigErrorMessage'));
  });

  // Test that when the add file label is clicked, the file dialog is opened and
  // the file input value will be reset to empty.
  test('canOpenFileDialogByClickAddFileLabel', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    const addFileLabel =
        strictQuery('#addFileLabel', page!.shadowRoot, HTMLElement);
    assertTrue(!!addFileLabel);
    const fileDialog =
        strictQuery('#selectFileDialog', page!.shadowRoot, HTMLInputElement);
    assertTrue(!!fileDialog);

    const blob = new Blob(
        [new Uint8Array(MAX_ATTACH_FILE_SIZE + 1)], {type: 'text/plain'});
    const fakeFile = new File([blob], 'fakeFile.txt');

    // Set the selected file manually to simulate a file has been selected.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(fakeFile);
    fileDialog.files = dataTransfer.files;
    // Verify that the file input has a value.
    assertEquals('C:\\fakepath\\fakeFile.txt', fileDialog.value);

    const fileDialogClickPromise = eventToPromise('click', fileDialog);
    let fileDialogClicked = false;
    fileDialog.addEventListener('click', () => {
      fileDialogClicked = true;
    });

    addFileLabel.click();

    await fileDialogClickPromise;
    assertTrue(fileDialogClicked);
    // Verify that the file input value has been reset to empty.
    assertEquals('', fileDialog.value);
  });

  // Test that when the replace file label is clicked, the file dialog is
  // opened.
  test('canOpenFileDialogByClickReplaceFileButton', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    const replaceFileButton =
        strictQuery('#replaceFileButton', page!.shadowRoot, HTMLElement);
    assertTrue(!!replaceFileButton);
    /**@type {!HTMLInputElement} */
    const fileDialog =
        /**@type {!HTMLInputElement} */ (
            strictQuery('#selectFileDialog', page!.shadowRoot, HTMLElement));
    assertTrue(!!fileDialog);

    const fileDialogClickPromise = eventToPromise('click', fileDialog);
    let fileDialogClicked = false;
    fileDialog.addEventListener('click', () => {
      fileDialogClicked = true;
    });

    replaceFileButton.click();

    await fileDialogClickPromise;
    assertTrue(fileDialogClicked);
  });

  // Test that replace file section is shown after a file is selected.
  test('showReplaceFile', async () => {
    await initializePage();

    // The addFileContainer should be visible when no file is selected.
    assertTrue(isVisible(getElement('#addFileContainer')));
    // The replaceFileContainer should be invisible when no file is selected.
    assertFalse(isVisible(getElement('#replaceFileContainer')));

    const blob = new Blob([new Uint8Array(100)], {type: 'application/zip'});
    const fakeFile = new File([blob], 'fake.zip');
    // Set selected file manually.
    page!.setSelectedFileForTesting(fakeFile);

    // The selected file name is set properly.
    assertEquals('fake.zip', getElementContent('#selectedFileName'));
    // The select file checkbox is checked automatically when a file is
    // selected.
    const selectFileCheckbox =
        strictQuery('#selectFileCheckbox', page!.shadowRoot, CrCheckboxElement);
    assertTrue(selectFileCheckbox.checked);
    assertEquals('Attach file', selectFileCheckbox.ariaDescription);

    // The addFileContainer should be invisible.
    assertFalse(isVisible(getElement('#addFileContainer')));
    // The replaceFileContainer should be visible.
    assertTrue(isVisible(getElement('#replaceFileContainer')));
    // The aria label of the replace file button is set.
    assertEquals(
        'Replace file',
        strictQuery('#replaceFileButton', page!.shadowRoot, HTMLButtonElement)
            .ariaLabel);
    // Verify the i18n string is added.
    assertTrue(page!.i18nExists('replaceFileLabel'));
    // Verify the image container is not visible for non-image files.
    assertFalse(isVisible(getElement('#selectedImageButton')));
  });

  // Test that when there is not a file selected, getAttachedFile returns null.
  test('hasNotSelectedAFile', async () => {
    await initializePage();

    // The selected file name is empty.
    assertEquals('', getElementContent('#selectedFileName'));
    // The select file checkbox is unchecked.
    assertFalse(
        strictQuery('#selectFileCheckbox', page!.shadowRoot, CrCheckboxElement)
            .checked);

    const attachedFile = await page!.getAttachedFile();
    assertEquals(null, attachedFile);
  });

  // Test that when a file was selected but the checkbox is unchecked,
  // getAttachedFile returns null.
  test('selectedAFileButUnchecked', async () => {
    await initializePage();

    const selectFileCheckbox =
        strictQuery('#selectFileCheckbox', page!.shadowRoot, CrCheckboxElement);
    // Set selected file manually.
    const blob = new Blob(
        [new Uint8Array(MAX_ATTACH_FILE_SIZE)], {type: 'application/zip'});
    const fakeFile = new File([blob], 'fake.zip');
    page!.setSelectedFileForTesting(fakeFile);
    selectFileCheckbox.checked = false;

    assertFalse(selectFileCheckbox.checked);
    assertEquals('fake.zip', getElementContent('#selectedFileName'));
    const attachedFile = await page!.getAttachedFile();
    assertEquals(null, attachedFile);
  });

  // Test that when a file was selected but the checkbox is checked,
  // getAttachedFile returns correct data.
  test('selectedAFileAndchecked', async () => {
    await initializePage();

    const selectFileCheckbox =
        strictQuery('#selectFileCheckbox', page!.shadowRoot, CrCheckboxElement);

    const blob = new Blob([new Uint8Array(100)], {type: 'application/zip'});
    const fakeFile = new File([blob], 'fake.zip');

    // Access the array buffer asynchronously
    fakeFile.arrayBuffer().then(buffer => {
      return buffer;
    });
    // Set selected file manually.
    page!.setSelectedFileForTesting(fakeFile);
    selectFileCheckbox.checked = true;

    assertEquals('fake.zip', getElementContent('#selectedFileName'));
    const attachedFile = await page!.getAttachedFile();
    // Verify the fileData field.
    assertEquals(100, attachedFile!.fileData!.bytes!.length);
    // Verify the fileName field.
    assertEquals('fake.zip', attachedFile!.fileName!.path!.path);
  });

  // Test that chosen file can' exceed 10MB.
  test('fileTooBig', async () => {
    await initializePage();

    const selectFileCheckbox =
        strictQuery('#selectFileCheckbox', page!.shadowRoot, CrCheckboxElement);

    const blob = new Blob(
        [new Uint8Array(MAX_ATTACH_FILE_SIZE + 1)], {type: 'application/zip'});
    const fakeFile = new File([blob], 'fake.zip');

    // Access the array buffer asynchronously
    fakeFile.arrayBuffer().then(buffer => {
      return buffer;
    });

    // Set selected file manually. It should be ignored.
    page!.setSelectedFileForTesting(fakeFile);
    selectFileCheckbox.checked = true;

    // Error message should be visible.
    assertTrue(
        strictQuery('#fileTooBigErrorMessage', page!.shadowRoot, CrToastElement)
            .open);
    assertEquals(
        `Can't upload file larger than 10 MB`,
        getElementContent('#fileTooBigErrorMessage > #errorMessage'));
    // There should not be a selected file.
    assertEquals('', getElementContent('#selectedFileName'));
    const attachedFile = await page!.getAttachedFile();
    // AttachedFile should be null.
    assertTrue(!attachedFile);
  });

  // Test that files that are image type have a preview.
  test('imageFilePreview', async () => {
    await initializePage();

    const blob = new Blob([new Uint8Array(100)], {type: 'image/png'});
    const fakeImageFile = new File([blob], 'fake.png', {type: 'image/png'});
    // Access the array buffer asynchronously
    fakeImageFile.arrayBuffer().then(_buffer => {
      return new Uint8Array([12, 11, 99]).buffer;
    });

    page!.setSelectedFileForTesting(fakeImageFile);
    await flushTasks();

    // The selected file name is set properly.
    assertEquals('fake.png', getElementContent('#selectedFileName'));

    // The selectedFileImage should have an url when file is image type.
    const imageUrl =
        strictQuery('#selectedFileImage', page!.shadowRoot, HTMLImageElement)
            .src;
    assertTrue(imageUrl.length > 0);
    // There should be a preview image.
    page!.setSelectedImageUrlForTesting(imageUrl);
    const selectedImage =
        strictQuery('#selectedFileImage', page!.shadowRoot, HTMLImageElement);
    assertTrue(!!selectedImage.src);
    assertEquals(imageUrl, selectedImage.src);
    const selectedImageButton = strictQuery(
        '#selectedImageButton', page!.shadowRoot, HTMLButtonElement);
    assertEquals('Preview fake.png', selectedImageButton.ariaLabel);
    // Verify the image container is visible for image files.
    assertTrue(isVisible(selectedImageButton));
  });

  /** Test that clicking the image will open preview dialog. */
  test('selectedImagePreviewDialog', async () => {
    await initializePage();
    verifyRecordPreSubmitActionCallCount(
        0, FeedbackAppPreSubmitAction.kViewedImage);

    const blob =
        new Blob([new Uint8Array(MAX_ATTACH_FILE_SIZE)], {type: 'image/png'});
    const fakeImageFile = new File([blob], 'fake.png');

    // Access the array buffer asynchronously
    fakeImageFile.arrayBuffer().then(buffer => {
      return buffer;
    });

    page!.setSelectedFileForTesting(fakeImageFile);
    page!.setSelectedImageUrlForTesting(fakeImageUrl);
    assertEquals(
        fakeImageUrl,
        strictQuery('#selectedFileImage', page!.shadowRoot, HTMLImageElement)
            .src);

    const closeDialogButton =
        strictQuery('#closeDialogButton', page!.shadowRoot, CrButtonElement);
    // The preview dialog's close icon button is not visible.
    assertFalse(isVisible(closeDialogButton));

    // The selectedImage is displayed as an image button.
    const imageButton = strictQuery(
        '#selectedImageButton', page!.shadowRoot, HTMLButtonElement);
    const imageClickPromise = eventToPromise('click', imageButton);
    imageButton.click();
    await imageClickPromise;

    verifyRecordPreSubmitActionCallCount(
        1, FeedbackAppPreSubmitAction.kViewedImage);

    // The preview dialog's title should be set properly.
    assertEquals('fake.png', getElementContent('#modalDialogTitleText'));

    // The preview dialog's close icon button is visible now.
    assertTrue(isVisible(closeDialogButton));
  });
});
