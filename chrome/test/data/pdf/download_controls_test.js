// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SaveRequestType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/constants.js';
import {ViewerDownloadControlsElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-download-controls.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const tests = [
  /**
   * Test that the toolbar shows an option to download the edited PDF if
   * available.
   */
  async function testEditedPdfOption() {
    document.body.innerHTML = '';
    /** @type {!ViewerDownloadControlsElement} */
    const downloadsElement = /** @type {!ViewerDownloadControlsElement} */ (
        document.createElement('viewer-download-controls'));
    downloadsElement.pdfFormSaveEnabled = false;
    downloadsElement.isFormFieldFocused = false;
    downloadsElement.hasEdits = false;
    downloadsElement.hasEnteredAnnotationMode = false;
    document.body.appendChild(downloadsElement);

    /** @type {!CrIconButtonElement} */
    const downloadButton = /** @type {!CrIconButtonElement} */ (
        downloadsElement.shadowRoot.querySelector('#download'));
    /** @type {!CrActionMenuElement} */
    const actionMenu = /** @type {!CrActionMenuElement} */ (
        downloadsElement.shadowRoot.querySelector('#menu'));
    chrome.test.assertFalse(actionMenu.open);

    let numRequests = 0;
    downloadsElement.addEventListener('save', () => numRequests++);

    /** @return {!Promise<SaveRequestType>} */
    const whenSave = function() {
      return new Promise(resolve => {
        listenOnce(downloadsElement, 'save', e => resolve(e.detail));
      });
    };

    /** @return {!Promise} */
    const whenDownloadMenuShown = function() {
      return new Promise(
          resolve => listenOnce(
              downloadsElement, 'download-menu-shown-for-testing', resolve));
    };

    // No edits, and feature is off.
    let onSave = whenSave();
    downloadButton.click();
    let requestType = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertEq(1, numRequests);

    // Still does not show the menu if there are no edits.
    downloadsElement.pdfFormSaveEnabled = true;
    onSave = whenSave();
    downloadButton.click();
    requestType = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertEq(2, numRequests);

    // Set form field focused.
    downloadsElement.isFormFieldFocused = true;
    onSave = whenSave();
    downloadButton.click();

    // Unfocus, without making any edits. Saves the original document.
    downloadsElement.isFormFieldFocused = false;
    requestType = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertEq(3, numRequests);

    // Focus again.
    downloadsElement.isFormFieldFocused = true;
    downloadButton.click();

    // Set editing mode and change the form focus. Now, the menu should
    // open.
    downloadsElement.hasEdits = true;
    downloadsElement.isFormFieldFocused = false;
    await whenDownloadMenuShown();
    chrome.test.assertTrue(actionMenu.open);
    chrome.test.assertEq(3, numRequests);

    // Click on "Edited".
    const buttons = downloadsElement.shadowRoot.querySelectorAll('button');
    onSave = whenSave();
    buttons[0].click();
    requestType = await onSave;
    chrome.test.assertEq(SaveRequestType.EDITED, requestType);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(4, numRequests);

    // Click again to re-open menu.
    downloadButton.click();
    await whenDownloadMenuShown();
    chrome.test.assertTrue(actionMenu.open);

    // Click on "Original".
    onSave = whenSave();
    buttons[1].click();
    requestType = await onSave;
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(5, numRequests);

    // Even if the document has been edited, always download the original
    // if the feature flag is off.
    downloadsElement.pdfFormSaveEnabled = false;
    onSave = whenSave();
    downloadButton.click();
    requestType = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertEq(6, numRequests);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
