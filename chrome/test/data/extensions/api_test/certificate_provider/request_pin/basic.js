// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SIGN_REQUEST_ID = 123;
const CORRECT_PIN = '1234';
const WRONG_ATTEMPTS_LIMIT = 3;

// The script requests pin and checks the input. If correct PIN (1234) is
// provided, the script requests to close the dialog and stops there. If wrong
// PIN is provided, the request is repeated until the limit of 3 bad tries is
// reached. If the dialog is closed, the request is repeated without considering
// it a wrong attempt. This allows the testing of quota limit of closed dialogs
// (3 closed dialogs per 10 minutes).
function onPinRequested(responseDetails) {
  if (chrome.runtime.lastError) {
    // Should end up here only when quota is exceeded.
    lastError = chrome.runtime.lastError.message;
    chrome.test.sendMessage(lastError);
    return;
  }

  if (attempts >= WRONG_ATTEMPTS_LIMIT) {
    chrome.test.sendMessage('No attempt left');
    return;
  }

  if (!responseDetails || !responseDetails.userInput) {
    chrome.certificateProvider.requestPin(
        {signRequestId: SIGN_REQUEST_ID}, onPinRequested);
    chrome.test.sendMessage('User closed the dialog', function(message) {
      if (message == 'GetLastError') {
        chrome.test.sendMessage(lastError);
      }
    });
    return;
  }

  const success = responseDetails.userInput == CORRECT_PIN;
  if (success) {
    chrome.certificateProvider.stopPinRequest(
        {signRequestId: SIGN_REQUEST_ID}, onPinRequestStopped);
    chrome.test.sendMessage(lastError == '' ? 'Success' : lastError);
  } else {
    attempts++;
    const code = attempts < WRONG_ATTEMPTS_LIMIT ?
        {signRequestId: SIGN_REQUEST_ID, errorType: 'INVALID_PIN'} :
        {
          signRequestId: SIGN_REQUEST_ID,
          requestType: 'PUK',
          errorType: 'MAX_ATTEMPTS_EXCEEDED',
          attemptsLeft: 0
        };
    chrome.certificateProvider.requestPin(code, onPinRequested);
    chrome.test.sendMessage(lastError == '' ? 'Invalid PIN' : lastError);
  }
}

function onPinRequestStopped() {
  if (chrome.runtime.lastError) {
    console.error('Error: ' + chrome.runtime.lastError.message);
    lastError = chrome.runtime.lastError.message;
  }
}

let attempts = 0;
let lastError = '';
chrome.certificateProvider.requestPin(
    {signRequestId: SIGN_REQUEST_ID}, onPinRequested);
