// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service Worker initialization listeners.
self.addEventListener('install', e => e.waitUntil(skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(clients.claim()));

// Posts |msg| to background_fetch.js.
function postToWindowClients(msg) {
  return clients.matchAll({ type: 'window' }).then(clientWindows => {
    for (const client of clientWindows) client.postMessage(msg);
  });
}

self.addEventListener('message', e => {
  const fetchPromise = self.registration.backgroundFetch.fetch(
    'sw-fetch', '/background_fetch/types_of_cheese.txt');
  if (e.data === 'fetchnowait')
    postToWindowClients('ok');
  else if (e.data === 'fetch')
    fetchPromise.catch(e => postToWindowClients('permissionerror'));
  else
    postToWindowClients('unexpected message');
});

// Background Fetch event listeners.
self.addEventListener('backgroundfetchsuccess', e => {
  e.waitUntil(e.updateUI({title: 'New Fetched Title!'}).then(
      () => postToWindowClients(e.type)));
});

self.addEventListener('backgroundfetchfail', e => {
  e.waitUntil(e.updateUI({title: 'New Failed Title!'}).then(
      () => postToWindowClients(e.type)));
});

self.addEventListener('backgroundfetchabort', e => {
  e.waitUntil(postToWindowClients(e.type));
});

self.addEventListener('backgroundfetchclick', e => {
  e.waitUntil(clients.openWindow(
      '/background_fetch/background_fetch.html?clickevent'));
});
