// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', event => {
  if (!event.request.url.endsWith('extension_page.html'))
    return;

  // Strip the CSP header from the request.
  return event.respondWith(
    fetch(event.request).then(response => {
      const kCSPHeader = 'content-security-policy';
      chrome.test.sendMessage(response.headers.get(kCSPHeader));
      let updatedHeaders = new Headers(response.headers);
      updatedHeaders.delete(kCSPHeader);
      let init = {
        status: response.status,
        statusText: response.statusText,
        headers: updatedHeaders
      };
      return new Response(response.body, init);
    })
  );
});

self.addEventListener('activate', event => {
  chrome.test.sendMessage('ready');
  return self.clients.claim();
});
