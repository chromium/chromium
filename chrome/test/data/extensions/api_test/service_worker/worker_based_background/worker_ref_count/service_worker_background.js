// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var numMessagesReceived = 0;
self.onmessage = function(e) {
  var fail = function() {
    e.ports[0].postMessage('FAILURE');
  };
  if (e.data == 'sendMessageTest') {
    try {
      chrome.test.sendMessage('CHECK_REF_COUNT', function(reply) {
        ++numMessagesReceived;
        // We expect two 'sendMessageTest' messages in the worker, reply to the
        // browser when we have received both.
        if (numMessagesReceived == 2) {
          chrome.test.sendMessage('SUCCESS_FROM_WORKER');
        }
        e.ports[0].postMessage('Worker reply: ' + reply);
      });
    } catch (e) {
      fail();
    }
  } else if (e.data == 'roundtrip-request') {
    e.ports[0].postMessage('roundtrip-response');
  } else {
    fail();
  }
};

chrome.test.sendMessage('WORKER STARTED');
