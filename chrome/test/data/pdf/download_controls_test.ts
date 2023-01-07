// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SaveRequestType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const tests = [
  /**
   * Test that the toolbar shows an option to download the edited PDF if
   * available.
   */
  async function testEditedPdfOption() {
    document.body.innerHTML = '';
    const downloadsElement = document.createElement('viewer-download-controls');
    downloadsElement.isFormFieldFocused = false;
    downloadsElement.hasEdits = false;
    downloadsElement.hasEnteredAnnotationMode = false;
    document.body.appendChild(downloadsElement);

    const downloadButton = downloadsElement.$.download;
    const actionMenu = downloadsElement.$.menu;
    chrome.test.assertFalse(actionMenu.open);

    let numRequests = 0;
    downloadsElement.addEventListener('save', () => numRequests++);

    // Do not show the menu if there are no edits.
    let onSave = eventToPromise('save', downloadsElement);
    downloadButton.click();
    let e: CustomEvent<SaveRequestType> = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);
    chrome.test.assertEq(1, numRequests);

    // Set form field focused.
    downloadsElement.isFormFieldFocused = true;
    onSave = eventToPromise('save', downloadsElement);
    downloadButton.click();

    // Unfocus, without making any edits. Saves the original document.
    downloadsElement.isFormFieldFocused = false;
    e = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);
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
    const buttons = downloadsElement.shadowRoot!.querySelectorAll('button');
    onSave = eventToPromise('save', downloadsElement);
    buttons[0]!.click();
    e = await onSave;
    chrome.test.assertEq(SaveRequestType.EDITED, e.detail);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(3, numRequests);

    // Click again to re-open menu.
    downloadButton.click();
    await eventToPromise('download-menu-shown-for-testing', downloadsElement);
    chrome.test.assertTrue(actionMenu.open);

    // Click on "Original".
    onSave = eventToPromise('save', downloadsElement);
    buttons[1]!.click();
    e = await onSave;
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(4, numRequests);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
