// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service Worker to be used with the platform_notification_service.html page.
var messagePort = null;

addEventListener('message', function (event) {
  messagePort = event.data;
  messagePort.postMessage('ready');
});

// The notificationclick event will be invoked when a persistent notification
// has been clicked on. When this happens, the title determines whether this
// Service Worker has to act upon this.
addEventListener('notificationclick', function (event) {
  if (event.notification.title == 'action_close')
    event.notification.close();

  var message = event.notification.title;

  if (message == 'action_button_click')
    message += ' ' + event.action;
  if (event.reply)
    message += ' ' + event.reply;
  messagePort.postMessage(message);
});

// The notificationclose event will be invoked when a persistent notification
// has been closed by the user.
addEventListener('notificationclose', function (event) {
  messagePort.postMessage('closing notification: ' + event.notification.title);
});
