// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let resolve;
const completionPromise = new Promise((res) => {
  resolve = res;
});

var messagesReceived = [];

function getMessageCountAfterReceivingRealSenderMessage() {
  return completionPromise;
}

chrome.runtime.onMessageExternal.addListener(function(msg, sender, respond) {
  messagesReceived.push({msg:msg, sender:sender});
  if (msg == 'from_sender') {
    resolve(messagesReceived.length);
  }
});

chrome.test.sendMessage('receiver_ready');