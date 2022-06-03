// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // This test has a few steps, which are outlined in order below:
  // 1. Start speech recognition.
  // 2. Once speech recognition has started, send a message to C++, which
  // will trigger a fake speech result to be sent.
  // 3. Once we get a speech result, let C++ know.
  // 4. Once C++ acknowledges our message, stop speech recognition.
  // 5. Pass this test once speech recognition has stopped.

  const clientId = 4;
  chrome.speechRecognitionPrivate.onResult.addListener((event) => {
    // Step 3.
    chrome.test.assertEq(clientId, event.clientId);
    chrome.test.assertEq('Testing', event.transcript);
    chrome.test.assertTrue(event.isFinal);
    chrome.test.sendMessage('Received result', async (proceed) => {
      // Steps 4 and 5.
      await chrome.speechRecognitionPrivate.stop({clientId});
      chrome.test.assertNoLastError();
      chrome.test.notifyPass();
    });
  });

  // Steps 1 and 2.
  await chrome.speechRecognitionPrivate.start({clientId});
  chrome.test.sendMessage('Started');
})();
