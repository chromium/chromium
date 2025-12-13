// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome://webui-test/test_util.js';

const SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;
type SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;

const tests = [
  /**
   * Test that the toolbar shows an option to download the edited PDF if
   * available.
   */
  async function testEditedPdfOption() {
    document.body.innerHTML = '';
    const downloadsElement = document.createElement('viewer-download-controls');
    document.body.appendChild(downloadsElement);

    const downloadButton = downloadsElement.$.save;
    const actionMenu = downloadsElement.$.menu;

    // Do not show the menu if there are no edits.
    let onSave = eventToPromise('save', downloadsElement);
    downloadButton.click();
    let e: CustomEvent<SaveRequestType> = await onSave;
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);

    // Set editing mode. Now, the menu should open.
    downloadsElement.hasEdits = true;
    downloadButton.click();
    await eventToPromise('save-menu-shown-for-testing', downloadsElement);
    chrome.test.assertTrue(actionMenu.open);

    // Click on "Edited".
    const buttons = downloadsElement.shadowRoot.querySelectorAll('button');
    onSave = eventToPromise('save', downloadsElement);
    buttons[0]!.click();
    e = await onSave;
    chrome.test.assertEq(SaveRequestType.EDITED, e.detail);

    // Click the button again to re-open menu and click on "Original".
    downloadButton.click();
    await eventToPromise('save-menu-shown-for-testing', downloadsElement);
    chrome.test.assertTrue(actionMenu.open);
    onSave = eventToPromise('save', downloadsElement);
    buttons[1]!.click();
    e = await onSave;
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
