// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', e => {
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
  const url = new URL(e.request.url);
  // Ignore requests with 'sync_signal' so they fall back to the standard
  // network stack. This guarantees the request is processed by the extension's
  // chrome.webRequest API, allowing deterministic test synchronization.
  if (url.searchParams.has('sync_signal')) {
    return;
  }
  if (url.searchParams.has('stream')) {
    let clientResolve;
    const clientPromise = new Promise(r => {
      clientResolve = r;
    });
    // Use a ReadableStream for the response body so we can keep the fetch
    // request pending until the test environment signals us to close it.
    // This provides a deterministic handshake to verify interception.
    const stream = new ReadableStream({
      start(controller) {
        const encoder = new TextEncoder();
        const html = `<head><title>SW Scope Page</title></head><body><script>
          window.addEventListener('message', e => {
            if (e.data === 'START') {
              e.ports[0].postMessage('SW_READY');
              e.ports[0].onmessage = (event) => {
                if (event.data === 'FINISH') {
                  const msg = 'SW_CLOSE_STREAM';
                  navigator.serviceWorker.controller.postMessage(msg);
                }
              };
            }
          });
        </script></body>`;
        controller.enqueue(encoder.encode(html));
        clientPromise.then(() => {
          controller.close();
        });
      }
    });

    self.addEventListener('message', event => {
      if (event.data === 'SW_CLOSE_STREAM') {
        clientResolve();
      }
    });

    e.respondWith(
        new Response(stream, {headers: {'Content-Type': 'text/html'}}));
    return;
  }

  const html = `
    <!DOCTYPE html>
      <html>
        <head><title>SW Scope Page</title></head>
        <body>Cached Response</body>
      </html>`;
  e.respondWith(new Response(html, {headers: {'Content-Type': 'text/html'}}));
});
