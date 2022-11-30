// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // This test has a few steps, which are outlined in order below:
  // 1. Start speech recognition.
  // 2. Once speech recognition has started, send a message to C++, which
  // will trigger a non-final speech result to be sent.
  // 3. Once we get a non-final speech result, let C++ know. A final speech
  // result will be sent after C++ knows that the non-final-result was received.
  // 4. Once we get a final speech result, let C++ know.
  // 5. C++ stops speech recognition.
  // 6. Pass this test once speech recognition has stopped.

  const clientId = 4;
  const interimResults = true;
  chrome.speechRecognitionPrivate.onResult.addListener((event) => {
    chrome.test.assertEq(clientId, event.clientId);
    const transcript = event.transcript;
    const isFinal = event.isFinal;
    if (transcript === 'First result') {
      // Step 3.
      chrome.test.assertFalse(isFinal);
      chrome.test.sendMessage('Received first result');
    } else if (transcript === 'Second result') {
      // Steps 4, 5, and 6.
      chrome.test.assertTrue(isFinal);
      chrome.test.sendMessage('Received second result', async (proceed) => {
        await chrome.speechRecognitionPrivate.stop({clientId});
        chrome.test.assertNoLastError();
        chrome.test.notifyPass();
      });
    } else {
      chrome.test.fail();
    }
  });

  // Steps 1 and 2.
  await chrome.speechRecognitionPrivate.start({clientId, interimResults});
  chrome.test.sendMessage('Started');
})();
