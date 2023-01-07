// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  const onDevice =
      chrome.speechRecognitionPrivate.SpeechRecognitionType.ON_DEVICE;
  const network = chrome.speechRecognitionPrivate.SpeechRecognitionType.NETWORK;
  const config = await chrome.test.getConfig();
  const expectedRecognitionType = config.customArg;
  const actualRecognitionType = await chrome.speechRecognitionPrivate.start({});
  chrome.test.assertTrue(
      actualRecognitionType === onDevice || actualRecognitionType === network);
  chrome.test.assertEq(expectedRecognitionType, actualRecognitionType);
  chrome.test.succeed();
})();
