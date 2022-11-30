// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', e => {
  e.respondWith((async () => {
    try {
      return await fetch(e.request);
    } catch (error) {
      return new Response('Hello offline page');
    }
  })());
});

self.addEventListener('push', event => {
  self.registration.showNotification('Hello world!', {
    body: 'Test Notification',
  });
});

self.addEventListener('notificationclick', event => {
  event.notification.close();
  event.waitUntil(self.clients.openWindow(
    self.location.origin + '/banners/isolated/register_service_worker.html'
  ));
});
