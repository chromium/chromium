// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', event => {
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', event => {
  // If a request arrives for `injected.js`, then provide replacement content.
  if (event.request.url.includes('injected.js')) {
    const replacementPayload =
        'window.postMessage(\'REPLACEMENT_SCRIPT\', \'*\');';
    event.respondWith(new Response(
        replacementPayload,
        {headers: {'Content-Type': 'application/javascript'}}));
  }
});
