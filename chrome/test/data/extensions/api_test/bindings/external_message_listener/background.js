// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var messagesReceived = [];

// Have we received the real message from the sender extension?
var receivedRealSenderMessage = false;

// Has the c++ code in the browser test asked us for the total count of messages
// we've received?
var sendCountAfterSenderMessage = false;

function getMessageCountAfterReceivingRealSenderMessage() {
  if (receivedRealSenderMessage) {
    window.domAutomationController.send(messagesReceived.length);
  } else {
    sendCountAfterSenderMessage = true;
  }
}

chrome.runtime.onMessageExternal.addListener(function(msg, sender, respond) {
  messagesReceived.push({msg:msg, sender:sender});
  if (msg == 'from_sender') {
    receivedRealSenderMessage = true;
    if (sendCountAfterSenderMessage) {
      window.domAutomationController.send(messagesReceived.length);
    }
  }
});

chrome.test.sendMessage('receiver_ready');