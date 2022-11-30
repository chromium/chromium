// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function onSendError() {
      var currentError = 0;
      var totalMessages = 0;
      var eventHandler = function(error) {
        chrome.test.assertEq(3, Object.keys(error.details).length);
        chrome.test.assertTrue(
            error.details.hasOwnProperty("expectedMessageId"));
        chrome.test.assertTrue(
            error.details.hasOwnProperty("expectedErrorMessage"));
        chrome.test.assertEq(error.details.expectedMessageId, error.messageId);
        chrome.test.assertEq(error.details.expectedErrorMessage,
                             error.errorMessage);
        currentError += 1;
        var tempTotalMessages = +error.details.totalMessages;
        if (totalMessages == 0)
          totalMessages = tempTotalMessages;
        else
          chrome.test.assertEq(totalMessages, tempTotalMessages);
        if (currentError == totalMessages) {
          chrome.gcm.onSendError.removeListener(eventHandler);
          chrome.test.succeed();
        }
      };
      chrome.gcm.onSendError.addListener(eventHandler);
    }
  ]);
};
