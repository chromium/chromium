// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const MESSAGE = {
  messageId: 'message-id',
  destinationId: 'destination-id',
  data: {
    key1: 'value1',
    key2: 'value2',
  },
};

chrome.test.runTests([
  function testSend() {
    chrome.gcm.send(MESSAGE, function(messageId) {
      chrome.test.succeed();
    });
  },
]);
