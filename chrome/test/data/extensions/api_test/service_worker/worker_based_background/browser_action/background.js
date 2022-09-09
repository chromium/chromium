// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var initialUserGesture = chrome.test.isProcessingUserGesture();

chrome.browserAction.onClicked.addListener(() => {
  chrome.test.assertFalse(initialUserGesture);

  // We should be running with a user gesture.
  // Note: isProcessingUserGesture() only performs renderer level
  // checks. There are other tests that call APIs that use actual
  // gesture.
  chrome.test.assertTrue(chrome.test.isProcessingUserGesture());
  // Call an API so we can check gesture state in the callback.
  chrome.tabs.create({url: chrome.runtime.getURL('page.html')}, () => {
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(chrome.test.isProcessingUserGesture());

    // Call another API from this callback, we shouldn't have any gestures
    // retained in this API's callback.
    chrome.tabs.create({url: 'about:blank'}, () => {
      chrome.test.assertNoLastError();
      chrome.test.assertFalse(chrome.test.isProcessingUserGesture());
      chrome.test.notifyPass();
    });
  });
});

chrome.test.sendMessage('ready');
