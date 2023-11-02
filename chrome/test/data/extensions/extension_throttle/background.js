// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a promise that resolves to whether the request was successfully
 * made. This will throw an error if the request finishes with any status
 * other than 503.
 * @return {Promise<bool>}
 */
function canMakeRequest(url) {
  return new Promise(function(resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function() {
      if (this.status == 503)
        resolve(true);
      else
        reject('Unexpected status: ' + this.status);
    };
    xhr.onerror = function() {
      resolve(false);
    };
    xhr.open('GET', url, /*async=*/true);
    xhr.send();
  });
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
  if (message.type == 'xhr') {
    runTest(message.url, message.requestsToMake,
            message.expectedFailRequestNum);
  } else {
    console.error('Unknown message: ' + JSON.stringify(message));
  }
});
