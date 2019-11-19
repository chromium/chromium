// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// By default there should not be any user gesture present.
chrome.test.assertFalse(chrome.test.isProcessingUserGesture());

// Note: showNotification() requires SW to be activated first.
self.onactivate = function(e) {
  self.registration.showNotification('Hello world',
      {body: 'Body here'})
      .then((e) => {
        chrome.test.notifyPass();
      }).catch((e) => {
        chrome.test.notifyFailure('showNotification failed');
      });
};

self.onnotificationclick = function(e) {
  chrome.test.log('onnotificationclick');
  // We should be running with a user gesture w.r.t. extension APIs.
  chrome.test.assertTrue(chrome.test.isProcessingUserGesture());
  chrome.tabs.create({url: 'about:blank'}, () => {
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(chrome.test.isProcessingUserGesture());

    // Call another API, this API's callback should not be running
    // with a gesture.
    chrome.tabs.create({url: 'about:blank'}, () => {
      chrome.test.assertNoLastError();
      chrome.test.assertFalse(chrome.test.isProcessingUserGesture());
      chrome.test.notifyPass();
    });
  });
};
