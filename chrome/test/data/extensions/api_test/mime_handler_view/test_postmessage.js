// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var messages = ['hey', 100, 25.0];
var messagesSent = 0;
var messagesReceived = 0;

window.addEventListener('message', function(event) {
  if (event.data == messages[messagesReceived]) {
    messagesReceived++;
    if (messagesReceived == messages.length)
      // Instruct the extension to call chrome.test.succeed().
      plugin.postMessage('succeed');
  } else {
    // Instruct the extension to call chrome.test.fail().
    plugin.postMessage('fail');
  }
}, false);

var plugin = document.getElementById('plugin');
function postNextMessage() {
  plugin.postMessage(messages[messagesSent]);
  messagesSent++;
  if (messagesSent < messages.length)
    setTimeout(postNextMessage, 0);
}
postNextMessage();
