// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {PDFViewerElement, ViewerPasswordDialogElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const viewer = /** @type {!PDFViewerElement} */ (
    document.body.querySelector('pdf-viewer'));

/** @return {!ViewerPasswordDialogElement} */
function getPasswordDialog() {
  return /** @type {!ViewerPasswordDialogElement} */ (
      viewer.shadowRoot.querySelector('#password-dialog'));
}

/** @return {!CrInputElement} */
function getPasswordInput() {
  return /** @type {!CrInputElement} */ (
      getPasswordDialog().shadowRoot.querySelector('#password'));
}

/** @return {!CrButtonElement} */
function getSubmitButton() {
  return /** @type {!CrButtonElement} */ (
      getPasswordDialog().shadowRoot.querySelector('#submit'));
}

function submitPassword() {
  const button = getSubmitButton();
  button.click();

  // The submit button and input field should both be disabled.
  chrome.test.assertTrue(button.disabled);
  chrome.test.assertTrue(getPasswordInput().disabled);
}

/** @param {string} password */
async function tryIncorrectPassword(password) {
  const input = getPasswordInput();
  input.value = password;

  const whenPasswordDenied =
      eventToPromise('password-denied-for-testing', getPasswordDialog());
  submitPassword();
  await whenPasswordDenied;

  // The input field should be marked invalid and re-enabled.
  chrome.test.assertTrue(input.invalid);
  chrome.test.assertFalse(input.disabled);

  // The submit button should be re-enabled.
  chrome.test.assertFalse(getSubmitButton().disabled);
}

/** @param {string} password */
async function tryCorrectPassword(password) {
  const input = getPasswordInput();
  input.value = password;

  // The dialog should close in response to a correct password.
  const whenDialogClosed = eventToPromise('close', getPasswordDialog());
  submitPassword();
  await whenDialogClosed;
  chrome.test.assertFalse(!!getPasswordDialog());
}

const tests = [
  async function testPasswordDialog() {
    const passwordDialog = getPasswordDialog();
    chrome.test.assertTrue(!!passwordDialog);

    // The input field should be marked valid and enabled.
    const input = getPasswordInput();
    chrome.test.assertTrue(!!input);
    chrome.test.assertFalse(input.invalid);
    chrome.test.assertFalse(input.disabled);

    // The submit button should be enabled.
    chrome.test.assertFalse(getSubmitButton().disabled);

    await tryIncorrectPassword('incorrect');
    await tryCorrectPassword('ownerpass');
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
