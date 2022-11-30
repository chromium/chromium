// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

self.onfetch = function(event) {
  let requestUrl = new URL(event.request.url);

  // Don't modify the content of the background page, because that will mess up
  // the test. This shouldn't be necessary! We're supposed to be bypassing the
  // SW when loading the background page. See crbug.com/532720.
  if (requestUrl.pathname.indexOf('/background.') != -1)
    return;

  // A Promise to the body response.
  let body = null;

  switch (requestUrl.pathname) {
    case '/background-client-is-awake':
      body = self.clients.matchAll({
        includeUncontrolled: true,
        type: 'window'
      }).then(function(clients) {
        return clients.find(function(client) {
          return new URL(client.url).pathname == '/background.html';
        }) ? 'true' : 'false';
      });
      break;

    case '/ping-background-client':
      body = chrome.runtime.getBackgroundClient().then(function(bg) {
        return new Promise(function(resolve) {
          let mc = new MessageChannel();
          mc.port1.onmessage = function(e) {
            mc.port1.onmessage = null;
            resolve(String(e.data == 'ping'));
          };
          bg.postMessage({echo: {port: mc.port2, message: 'ping'}}, [mc.port2]);
        });
      });
      break;

    default:
      body = Promise.resolve('Unrecognized command: ' + requestUrl.pathname);
      break;
  }

  event.respondWith(body.then(function(bodyContent) {
    return new Response(bodyContent);
  }));
};
