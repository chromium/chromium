// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileAttachmentElement} from 'chrome://os-feedback/file_attachment.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {eventToPromise, flushTasks, isVisible} from '../../test_util.js';

export function fileAttachmentTestSuite() {
  /** @type {?FileAttachmentElement} */
  let page = null;

  setup(() => {
    document.body.innerHTML = '';
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

  // Test the page is loaded with expected HTML elements.
  test('elementLoaded', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    assertEquals('Add file', getElementContent('#addFileLabel'));
    // The addFileContainer should be visible when no file is selected.
    assertTrue(isVisible(getElement('#addFileContainer')));
    // The replaceFileContainer should be invisible when no file is selected.
    assertFalse(isVisible(getElement('#replaceFileContainer')));
  });

  // Test that when the add file label is clicked, the file dialog is opened.
  test('canOpenFileDialogByClickAddFileLabel', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    const addFileLabel = getElement('#addFileLabel');
    assertTrue(!!addFileLabel);
    /**@type {!HTMLInputElement} */
    const fileDialog =
        /**@type {!HTMLInputElement} */ (getElement('#selectFileDialog'));
    assertTrue(!!fileDialog);

    const fileDialogClickPromise = eventToPromise('click', fileDialog);
    let fileDialogClicked = false;
    fileDialog.addEventListener('click', (event) => {
      fileDialogClicked = true;
    });

    addFileLabel.click();

    await fileDialogClickPromise;
    assertTrue(fileDialogClicked);
  });

  // Test that when the replace file label is clicked, the file dialog is
  // opened.
  test('canOpenFileDialogByClickReplaceFileLabel', async () => {
    await initializePage();
    // Verify the add file label is in the page.
    const replaceFileLabel = getElement('#replaceFileLabel');
    assertTrue(!!replaceFileLabel);
    /**@type {!HTMLInputElement} */
    const fileDialog =
        /**@type {!HTMLInputElement} */ (getElement('#selectFileDialog'));
    assertTrue(!!fileDialog);

    const fileDialogClickPromise = eventToPromise('click', fileDialog);
    let fileDialogClicked = false;
    fileDialog.addEventListener('click', (event) => {
      fileDialogClicked = true;
    });

    replaceFileLabel.click();

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
    const fakeFile = /** @type {!File} */ ({name: 'fake.zip'});
    page.setSelectedFileForTesting(fakeFile);

    assertEquals('fake.zip', page.selectedFile.name);
    // The addFileContainer should be invisible.
    assertFalse(isVisible(getElement('#addFileContainer')));
    // The replaceFileContainer should be visible.
    assertTrue(isVisible(getElement('#replaceFileContainer')));
  });
}
