// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', function(event) {
  event.waitUntil(
    (async() => {
       // Find the associated background page from the active clients.
       let foundClients =
           await clients.matchAll({includeUncontrolled: true, type: 'window'});
       let background =
           foundClients.find((client) => {
             return new URL(client.url).pathname ==
                 '/_generated_background_page.html';
           });
        // Ensure that the "seenUrls" list in the background page is updated
        // before the response is served.
        await new Promise((resolve) => {
          var chan = new MessageChannel();
          chan.port1.onmessage = resolve;
          background.postMessage(event.request.url, [chan.port2]);
        });
    })());
});
