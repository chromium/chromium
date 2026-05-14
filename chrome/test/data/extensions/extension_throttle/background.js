// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns whether the request was successfully made. Throws an error if the
 * request finishes with any status other than 503.
 * @return {bool}
 */
async function canMakeRequest(url) {
  try {
    const response = await fetch(url);
    if (response.status === 503) {
      return true;
    } else {
      throw new Error('Unexpected status: ' + response.status);
    }
  } catch (e) {
    return false;
  }
}

async function runTest(url, requestsToMake, expectedFailRequestNum) {
  for (let i = 1; i <= requestsToMake; i += 1) {
    try {
      const madeRequest = await canMakeRequest(url);
      const expectSuccess = i < expectedFailRequestNum;
      chrome.test.assertEq(expectSuccess, madeRequest);
    } catch (e) {
      chrome.test.notifyFail('Error: ' + e.message);
    }
  }
  chrome.test.notifyPass('test passed');
}

chrome.runtime.onMessage.addListener(function(message, sender, sendResponse) {
  if (message.type === 'xhr') {
    runTest(
        message.url, message.requestsToMake, message.expectedFailRequestNum);
  } else {
    console.error('Unknown message: ' + JSON.stringify(message));
  }
});
