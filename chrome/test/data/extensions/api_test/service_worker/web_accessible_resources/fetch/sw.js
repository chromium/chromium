// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onfetch = function(e) {
  var url = new URL(e.request.url);
  if (url.pathname == '/data_for_extension') {
    e.respondWith(new Response('SW served data'));
  }
};

self.onmessage = function(e) {
  var request = e.data;
  switch (request) {
    case 'claim':
      self.clients.claim().then(function() {
        e.ports[0].postMessage('clients claimed');
      }).catch(function(err) {
        e.ports[0].postMessage('FAIL' + err);
      });
      break;
    default:
      e.ports[0].postMessage('FAIL: Incorrect request.');
      break;
  }
};
