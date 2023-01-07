// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service Worker to be used with the platform_notification_service.html page.
var messagePort = null;

addEventListener('message', async event => {
  if (event.data instanceof MessagePort) {
    messagePort = event.data;
    messagePort.postMessage('ready');
  }

  const message = event.data;
  switch (message) {
    case 'getServiceWorkerNotificationPermission':
      messagePort.postMessage(message + ':' + Notification.permission);
      break;

    case 'queryServiceWorkerNotificationPermission': {
      let result = await navigator.permissions.query({name: 'notifications'});
      messagePort.postMessage(message + ':' + result.state);
      break;
    }

    case 'getServiceWorkerPushPermission': {
      let pushManager = self.registration.pushManager;
      let result = await pushManager.permissionState({userVisibleOnly:true});
      messagePort.postMessage(message + ':' + result);
      break;
    }
  }
});
