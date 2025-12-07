// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerSaveControlsMixin} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {CrActionMenuElement, CrIconButtonElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;
const TestElementBase = ViewerSaveControlsMixin(CrLitElement);
type SaveRequestType = chrome.pdfViewerPrivate.SaveRequestType;

interface TestElement {
  $: {
    save: CrIconButtonElement,
    menu: CrActionMenuElement,
  };
}

class TestElement extends TestElementBase {
  static get is() {
    return 'test-element';
  }

  override render() {
    return html`
      <cr-icon-button id="save"></cr-icon-button>
      <cr-action-menu id="menu"></cr-action-menu>`;
  }

  override getMenu(): CrActionMenuElement {
    return this.$.menu;
  }

  override getSaveButton(): CrIconButtonElement {
    return this.$.save;
  }

  override getSaveEventType(): string {
    return 'save';
  }
}

const tests = [
  /**
   * Test that the toolbar shows an option to download the edited PDF if
   * available.
   */
  async function testEditedPdfOption() {
    customElements.define(TestElement.is, TestElement);
    const testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
    const actionMenu = testElement.getMenu();

    let numRequests: number = 0;
    testElement.addEventListener('save', () => numRequests++);
    chrome.test.assertFalse(actionMenu.open);

    // Call `onSaveClick` without any edits.
    let onSave = eventToPromise('save', testElement);
    testElement.onSaveClick();
    let e: CustomEvent<SaveRequestType> = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);
    chrome.test.assertEq(1, numRequests);

    // Set form field focused.
    testElement.isFormFieldFocused = true;
    onSave = eventToPromise('save', testElement);
    testElement.onSaveClick();

    // Unfocus, without making any edits. Saves the original document.
    testElement.isFormFieldFocused = false;
    e = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);
    chrome.test.assertEq(2, numRequests);

    // Focus again.
    testElement.isFormFieldFocused = true;
    testElement.onSaveClick();

    // Set editing mode and change the form focus. Now, the menu should
    // open.
    testElement.hasEdits = true;
    testElement.isFormFieldFocused = false;
    await eventToPromise('save-menu-shown-for-testing', testElement);
    chrome.test.assertTrue(actionMenu.open);
    chrome.test.assertEq(2, numRequests);

    // Save "Edited".
    onSave = eventToPromise('save', testElement);
    testElement.onSaveEditedClick();
    e = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.EDITED, e.detail);
    chrome.test.assertEq(3, numRequests);

    // Save "Original".
    onSave = eventToPromise('save', testElement);
    testElement.onSaveOriginalClick();
    e = await onSave;
    chrome.test.assertFalse(actionMenu.open);
    chrome.test.assertEq(SaveRequestType.ORIGINAL, e.detail);
    chrome.test.assertEq(4, numRequests);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
