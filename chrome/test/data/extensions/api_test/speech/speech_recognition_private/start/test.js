// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([async function startTest() {
  await chrome.speechRecognitionPrivate.start(
    {locale: 'en-US', interimResults: true});
  // Call the API again with different properties.
  // Keep the locale as en-US, otherwise the the on-device variant of this
  // test will fail because on-device speech recognition is only supported
  // in en-US.
  await chrome.speechRecognitionPrivate.start({interimResults: false});
  chrome.test.succeed();
}]);
