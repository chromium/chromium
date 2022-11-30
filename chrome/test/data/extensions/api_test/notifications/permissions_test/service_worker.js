// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let messagePort = null;

addEventListener('install', event => {
  event.waitUntil(skipWaiting());
});

addEventListener('activate', event => {
  event.waitUntil(clients.claim());
});

addEventListener('message', event => {
  messagePort = event.data;

  // Listens to incoming instructions from the hosting background script.
  messagePort.onmessage = async messageEvent => {
    switch (messageEvent.data) {
      case 'notification-permission':
        messagePort.postMessage(Notification.permission);
        break;
      case 'pushManager-permissionState':
        messagePort.postMessage(await registration.pushManager.permissionState({
          userVisibleOnly: true,
        }));
        break;
      case 'permissions-query':
        const permissionState = await navigator.permissions.query({
          name: 'notifications',
        });

        messagePort.postMessage(permissionState.state);
        break;
    }
  };

  messagePort.postMessage('ready');
});
