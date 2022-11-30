// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var messagePort = null;

addEventListener('message', function(event) {
  messagePort = event.data;
  messagePort.postMessage('ready');
});

addEventListener('notificationclick', function(event) {
  if (event.notification.data === 'ACTION_CREATE_TAB')
    clients.openWindow('https://chrome.com/');
  else if (event.notification.data === 'ACTION_REPLY')
    messagePort.postMessage('reply: ' + event.reply)

  event.notification.close();
});
