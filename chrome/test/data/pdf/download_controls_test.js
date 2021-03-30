// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {SaveRequestType, ViewerDownloadControlsElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
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

    // Do not show the menu if there are no edits.
    let onSave = whenSave();
    downloadButton.click();
    let requestType = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertEq(1, numRequests);

    // Set form field focused.
    downloadsElement.isFormFieldFocused = true;
    onSave = whenSave();
    downloadButton.click();

    // Unfocus, without making any edits. Saves the original document.
    downloadsElement.isFormFieldFocused = false;
    requestType = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertEq(2, numRequests);

    // Focus again.
    downloadsElement.isFormFieldFocused = true;
    downloadButton.click();

    // Set editing mode and change the form focus. Now, the menu should
    // open.
    downloadsElement.hasEdits = true;
    downloadsElement.isFormFieldFocused = false;
    await eventToPromise('download-menu-shown-for-testing', downloadsElement);
    chrome.test.assertTrue(actionMenu.open);
    chrome.test.assertEq(2, numRequests);

    // Click on "Edited".
    const buttons = downloadsElement.shadowRoot.querySelectorAll('button');
    onSave = whenSave();
    buttons[0].click();
    requestType = await onSave;
    chrome.test.assertEq(SaveRequestType.EDITED, requestType);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(3, numRequests);

    // Click again to re-open menu.
    downloadButton.click();
    await eventToPromise('download-menu-shown-for-testing', downloadsElement);
    chrome.test.assertTrue(actionMenu.open);

    // Click on "Original".
    onSave = whenSave();
    buttons[1].click();
    requestType = await onSave;
    chrome.test.assertEq(SaveRequestType.ORIGINAL, requestType);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(4, numRequests);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
