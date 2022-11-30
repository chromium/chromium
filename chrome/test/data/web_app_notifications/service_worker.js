// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', function() {
  // Activate worker immediately.
  return self.skipWaiting();
});

self.addEventListener('activate', function() {
  // Become available to all pages.
  return self.clients.claim();
});

self.addEventListener('fetch', function(event) {
  event.respondWith(fetch(event.request));
});

async function displayPersistentNotification(message) {
  var options = {
    body: 'Notification Body',
    vibrate: [100, 50, 100],
    data: {dateOfArrival: Date.now(), primaryKey: 1}
  };

  if (message.useBadge) {
    options.badge = 'blue-32.png';
  }

  return self.registration.showNotification(message.title, options);
}

self.addEventListener('message', async (event) => {
  let message = event.data;
  let responsePort = event.ports[0];

  if (message.type === 'showNotification') {
    await displayPersistentNotification(message);
    responsePort.postMessage({result: true});
  }

  if (message.type === 'closeAllNotifications') {
    let notifications = await self.registration.getNotifications();
    for (notification of notifications) {
      notification.close();
    }
    responsePort.postMessage({result: true});
  }
});
