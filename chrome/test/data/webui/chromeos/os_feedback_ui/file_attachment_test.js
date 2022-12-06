// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';

import {FakeFeedbackServiceProvider} from 'chrome://os-feedback/fake_feedback_service_provider.js';
import {FeedbackAppPreSubmitAction} from 'chrome://os-feedback/feedback_types.js';
import {FileAttachmentElement} from 'chrome://os-feedback/file_attachment.js';
import {setFeedbackServiceProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {mojoString16ToString} from 'chrome://resources/ash/common/mojo_utils.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {eventToPromise, isVisible} from '../test_util.js';

/** @type {string} */
const fakeImageUrl = 'chrome://os_feedback/app_icon_48.png';

const MAX_ATTACH_FILE_SIZE = 10 * 1024 * 1024;

export function fileAttachmentTestSuite() {
  /** @type {?FileAttachmentElement} */
  let page = null;

  /** @type {?FakeFeedbackServiceProvider} */
  let feedbackServiceProvider;

  setup(() => {
    document.body.innerHTML = '';
    feedbackServiceProvider = new FakeFeedbackServiceProvider();
    setFeedbackServiceProviderForTesting(feedbackServiceProvider);
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!FileAttachmentElement} */ (
            document.createElement('file-attachment'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /**
   * @param {string} selector
   * @returns {?Element}
   */
  function getElement(selector) {
    const element = page.shadowRoot.querySelector(selector);
    return element;
  }

  /**
   * @param {string} selector
   * @returns {string}
   */
  function getElementContent(selector) {
    const element = getElement(selector);
    assertTrue(!!element);
    return element.textContent.trim();
  }

  /**
   * @param {number} callCounts
   * @param {FeedbackAppPreSubmitAction} action
   * @private
   */
  function verifyRecordPreSubmitActionCallCount(callCounts, action) {
    assertEquals(
        callCounts,
        feedbackServiceProvider.getRecordPreSubmitActionCallCount(action));
  }

  // Test the page is loaded with expected HTML elements.
  test('elementLoaded', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    assertEquals('Add file', getElementContent('#addFileLabel'));
    // Verify the i18n string is added.
    assertTrue(page.i18nExists('addFileLabel'));
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
    const addFileLabel = getElement('#addFileLabel');
    assertTrue(!!addFileLabel);
    /**@type {!HTMLInputElement} */
    const fileDialog =
        /**@type {!HTMLInputElement} */ (getElement('#selectFileDialog'));
    assertTrue(!!fileDialog);

    // Create a new fake File object
    const fakeFile = new File(
        ['This is a fake file!'], 'fakeFile.txt',
        /** @type {FilePropertyBag} */ ({
          type: 'text/plain',
          size: MAX_ATTACH_FILE_SIZE + 1,
          lastModified: new Date(),
        }));
    // Set the selected file manually to simulate a file has been selected.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(fakeFile);
    fileDialog.files = dataTransfer.files;
    // Verify that the file input has a value.
    assertEquals('C:\\fakepath\\fakeFile.txt', fileDialog.value);

    const fileDialogClickPromise = eventToPromise('click', fileDialog);
    let fileDialogClicked = false;
    fileDialog.addEventListener('click', (event) => {
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
    const replaceFileButton = getElement('#replaceFileButton');
    assertTrue(!!replaceFileButton);
    /**@type {!HTMLInputElement} */
    const fileDialog =
        /**@type {!HTMLInputElement} */ (getElement('#selectFileDialog'));
    assertTrue(!!fileDialog);

    const fileDialogClickPromise = eventToPromise('click', fileDialog);
    let fileDialogClicked = false;
    fileDialog.addEventListener('click', (event) => {
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

    // Set selected file manually.
    /** @type {!File} */
    const fakeFile = /** @type {!File} */ ({
      name: 'fake.zip',
      type: 'application/zip',
    });
    page.setSelectedFileForTesting(fakeFile);

    // The selected file name is set properly.
    assertEquals('fake.zip', getElementContent('#selectedFileName'));
    // The select file checkbox is checked automatically when a file is
    // selected.
    const selectFileCheckbox = getElement('#selectFileCheckbox');
    assertTrue(selectFileCheckbox.checked);
    assertEquals('Attach file', selectFileCheckbox.ariaDescription);

    // The addFileContainer should be invisible.
    assertFalse(isVisible(getElement('#addFileContainer')));
    // The replaceFileContainer should be visible.
    assertTrue(isVisible(getElement('#replaceFileContainer')));
    // The aria label of the replace file button is set.
    assertEquals('Replace file', getElement('#replaceFileButton').ariaLabel);
    // Verify the i18n string is added.
    assertTrue(page.i18nExists('replaceFileLabel'));
    // Verify the image container is not visible for non-image files.
    const selectedImageButton = getElement('#selectedImageButton');
    assertFalse(isVisible(selectedImageButton));
  });

  // Test that when there is not a file selected, getAttachedFile returns null.
  test('hasNotSelectedAFile', async () => {
    await initializePage();

    // The selected file name is empty.
    assertEquals('', getElementContent('#selectedFileName'));
    // The select file checkbox is unchecked.
    assertFalse(getElement('#selectFileCheckbox').checked);

    const attachedFile = await page.getAttachedFile();
    assertEquals(null, attachedFile);
  });

  // Test that when a file was selected but the checkbox is unchecked,
  // getAttachedFile returns null.
  test('selectedAFileButUnchecked', async () => {
    await initializePage();

    const selectFileCheckbox = getElement('#selectFileCheckbox');
    // Set selected file manually.
    /** @type {!File} */
    const fakeFile = /** @type {!File} */ ({
      name: 'fake.zip',
      type: 'application/zip',
      size: MAX_ATTACH_FILE_SIZE,
    });
    page.setSelectedFileForTesting(fakeFile);
    selectFileCheckbox.checked = false;

    assertFalse(selectFileCheckbox.checked);
    assertEquals('fake.zip', getElementContent('#selectedFileName'));
    const attachedFile = await page.getAttachedFile();
    assertEquals(null, attachedFile);
  });

  // Test that when a file was selected but the checkbox is checked,
  // getAttachedFile returns correct data.
  test('selectedAFileAndchecked', async () => {
    await initializePage();

    const selectFileCheckbox = getElement('#selectFileCheckbox');
    const fakeData = [12, 11, 99];

    /** @type {!File} */
    const fakeFile = /** @type {!File} */ ({
      name: 'fake.zip',
      type: 'application/zip',
      size: MAX_ATTACH_FILE_SIZE,
      arrayBuffer: async () => {
        return new Uint8Array(fakeData).buffer;
      },
    });
    // Set selected file manually.
    page.setSelectedFileForTesting(fakeFile);
    selectFileCheckbox.checked = true;

    assertEquals('fake.zip', getElementContent('#selectedFileName'));
    const attachedFile = await page.getAttachedFile();
    // Verify the fileData field.
    assertTrue(!!attachedFile);
    assertTrue(!!attachedFile.fileData);
    assertTrue(!!attachedFile.fileData.bytes);
    assertArrayEquals(
        fakeData, /** @type {!Array<Number>} */ (attachedFile.fileData.bytes));
    // Verify the fileName field.
    assertEquals('fake.zip', attachedFile.fileName.path.path);
  });

  // Test that chosen file can' exceed 10MB.
  test('fileTooBig', async () => {
    await initializePage();

    const selectFileCheckbox = getElement('#selectFileCheckbox');
    const fakeData = [12, 11, 99];

    /** @type {!File} */
    const fakeFile = /** @type {!File} */ ({
      name: 'fake.zip',
      type: 'application/zip',
      size: MAX_ATTACH_FILE_SIZE + 1,
      arrayBuffer: async () => {
        return new Uint8Array(fakeData).buffer;
      },
    });
    // Set selected file manually. It should be ignored.
    page.setSelectedFileForTesting(fakeFile);
    selectFileCheckbox.checked = true;

    // Error message should be visible.
    assertTrue(getElement('#fileTooBigErrorMessage').open);
    assertEquals(
        `Can't upload file larger than 10 MB`,
        getElementContent('#fileTooBigErrorMessage > #errorMessage'));
    // There should not be a selected file.
    assertEquals('', getElementContent('#selectedFileName'));
    const attachedFile = await page.getAttachedFile();
    // AttachedFile should be null.
    assertTrue(!attachedFile);
  });

  // Test that files that are image type have a preview.
  test('imageFilePreview', async () => {
    await initializePage();
    const selectFileCheckbox = getElement('#selectFileCheckbox');
    const fakeData = [12, 11, 99];

    /** @type {!File} */
    const fakeImageFile = /** @type {!File} */ ({
      name: 'fake.png',
      type: 'image/png',
      size: MAX_ATTACH_FILE_SIZE,
      arrayBuffer: async () => {
        return new Uint8Array(fakeData).buffer;
      },
    });

    page.setSelectedFileForTesting(fakeImageFile);
    await flushTasks();

    // The selected file name is set properly.
    assertEquals('fake.png', getElementContent('#selectedFileName'));

    // The selectedFileImage should have an url when file is image type.
    const imageUrl = getElement('#selectedFileImage').src;
    assertTrue(imageUrl.length > 0);
    // There should be a preview image.
    page.selectedImageUrl_ = imageUrl;
    const selectedImage = getElement('#selectedFileImage');
    assertTrue(!!selectedImage.src);
    assertEquals(imageUrl, selectedImage.src);
    const selectedImageButton = getElement('#selectedImageButton');
    assertEquals('Preview fake.png', selectedImageButton.ariaLabel);
    // Verify the image container is visible for image files.
    assertTrue(isVisible(selectedImageButton));
  });

  // Test that clicking the image will open preview dialog and set the
  // focus on the close dialog icon button.
  test('selectedImagePreviewDialog', async () => {
    await initializePage();
    verifyRecordPreSubmitActionCallCount(
        0, FeedbackAppPreSubmitAction.kViewedImage);
    const fakeData = [12, 11, 99];

    /** @type {!File} */
    const fakeImageFile = /** @type {!File} */ ({
      name: 'fake.png',
      type: 'image/png',
      size: MAX_ATTACH_FILE_SIZE,
      arrayBuffer: async () => {
        return new Uint8Array(fakeData).buffer;
      },
    });

    page.setSelectedFileForTesting(fakeImageFile);
    page.selectedImageUrl_ = fakeImageUrl;
    assertEquals(fakeImageUrl, getElement('#selectedFileImage').src);

    const closeDialogButton = getElement('#closeDialogButton');
    // The preview dialog's close icon button is not visible.
    assertFalse(isVisible(closeDialogButton));

    // The selectedImage is displayed as an image button.
    const imageButton =
        /** @type {!Element} */ (getElement('#selectedImageButton'));
    const imageClickPromise = eventToPromise('click', imageButton);
    imageButton.click();
    await imageClickPromise;

    verifyRecordPreSubmitActionCallCount(
        1, FeedbackAppPreSubmitAction.kViewedImage);

    // The preview dialog's title should be set properly.
    assertEquals('fake.png', getElementContent('#modalDialogTitleText'));

    // The preview dialog's close icon button is visible now.
    assertTrue(isVisible(closeDialogButton));
    // The preview dialog's close icon button is focused.
    assertEquals(closeDialogButton, getDeepActiveElement());

    // Press enter should close the preview dialog.
    closeDialogButton.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    // The preview dialog's close icon button is not visible now.
    assertFalse(isVisible(closeDialogButton));
  });
}
