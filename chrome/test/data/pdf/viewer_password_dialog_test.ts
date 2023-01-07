// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;

function getPasswordDialog() {
  return viewer.shadowRoot!.querySelector('viewer-password-dialog')!;
}

function getPasswordInput() {
  return getPasswordDialog().$.password;
}

function getSubmitButton() {
  return getPasswordDialog().$.submit;
}

function submitPassword() {
  const button = getSubmitButton();
  button.click();

  // The submit button and input field should both be disabled.
  chrome.test.assertTrue(button.disabled);
  chrome.test.assertTrue(getPasswordInput().disabled);
}

async function tryIncorrectPassword(password: string) {
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

async function tryCorrectPassword(password: string) {
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
