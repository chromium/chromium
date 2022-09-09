// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var otherId = 'dceacbkfkmllgmjmbhgkpjegnodmildf';

function testConversation(port) {
  var sawResponses = false;
  port.onMessage.addListener(function(msg) {
    if (msg == 'message_1_reply') {
      port.postMessage('message_2');
    } else if (msg == 'message_2_reply') {
      sawResponses = true;
      port.postMessage('ok_to_disconnect');
    } else {
      console.log('saw unexpected message: "' + msg + '"');
      chrome.test.fail();
    }
  });
  port.onDisconnect.addListener(function(){
    if (sawResponses)
      chrome.test.succeed();
    else
      chrome.test.fail();
  });
  port.postMessage('message_1');
}

chrome.test.sendMessage('Launched');

chrome.test.runTests([

  function connect() {
    var port = chrome.runtime.connect(otherId);
    testConversation(port);
  },

  function connectUsingNamedPort() {
    var port = chrome.runtime.connect(otherId, {'name': 'SomeChannelName'});
    testConversation(port);
  },

  function sendMessage() {
    chrome.runtime.sendMessage(otherId, 'hello', function(response) {
      if (response == 'hello_response')
        chrome.test.succeed();
      else
        chrome.test.fail();
    });
  }

]);
