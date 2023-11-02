// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', function(event) {
    if (event.request.url.indexOf('sw_controlled_check') != -1) {
      event.respondWith(new Response('SW controlled'));
    } else if (event.request.url.indexOf('data_for_content_script') != -1) {
      event.respondWith(new Response('SW served data'));
    }
  });

self.addEventListener('message', function(event) {
    self.clients.claim()
      .then(function(result) {
          event.data.port.postMessage('clients claimed');
        })
      .catch(function(error) {
          event.data.port.postMessage('FAIL: exception: ' + error.name);
        });
  });
