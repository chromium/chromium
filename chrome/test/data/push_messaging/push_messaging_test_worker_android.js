// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The MessagePort to communicate with the client.
var messagePort = null;

// If true this service worker will show a notification when a push message is
// received.
var notifyOnPush = true;

// The number of notifications shown.
var notificationCounter = 0;

// Sends a message to the test, via the page.
function sendToTest(message) {
  messagePort.postMessage(JSON.stringify({
    'type': 'sendToTest',
    'data': message
  }));
}

self.onmessage = event => {
  if (event.data instanceof MessagePort) {
    messagePort = event.data;
    messagePort.postMessage('ready');
    return;
  }

  var message = JSON.parse(event.data);
  if (message.type == 'setNotifyOnPush') {
    notifyOnPush = message.data;
    sendToTest('setNotifyOnPush ' + message.data + ' ok');
    return;
  }

  sendToTest('Unknown message type.');
};

self.onpush = event => {
  if (notifyOnPush) {
    notificationCounter++;
    event.waitUntil(registration.showNotification(
        'push notification ' + notificationCounter));
  }
};
