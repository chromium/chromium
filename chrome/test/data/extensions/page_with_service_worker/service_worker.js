// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', event => {
  // Skip waiting to activate the new service worker immediately.
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  // Take control of the open page immediately.
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);

  // Serve the result only for the expected resource.
  if (!url.pathname.endsWith('style.css')) {
    event.respondWith(fetch(event.request));
    return;
  }

  const cssContent = `
body {
  color: __MSG_color__;
}
`;

  const response =
      new Response(cssContent, {headers: {'Content-Type': 'text/css'}});

  // Respond to the fetch event with our custom response.
  event.respondWith(response);
});
