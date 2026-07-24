// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function handleFetch(e) {
  try {
    const response = await fetch(e.data.url);
    if (!response.ok) {
      e.ports[0].postMessage('bad response');
      return;
    }
    const text = await response.text();
    e.ports[0].postMessage(text);
  } catch (error) {
    e.ports[0].postMessage(`${error}`);
  }
}

async function handleNavigate(e) {
  try {
    const navigation =
        clients.matchAll({type: 'window'}).then(function(clientList) {
          if (clientList.length === 0) {
            e.ports[0].postMessage('Error: no clients');
            return;
          }

          var firstClient = clientList[0];
          return firstClient.navigate(e.data.url);
        });
    e.ports[0].postMessage('success');
  } catch (error) {
    e.ports[0].postMessage(`${error}`);
  }
}

async function handleMessage(e) {
  switch (e.data.method) {
    case 'navigate':
      handleNavigate(e);
      break;
    case 'fetch':
    default:
      handleFetch(e);
      break;
  }
}

self.addEventListener('activate', function(event) {
  event.waitUntil(clients.claim());
});

self.addEventListener('message', e => {
  e.waitUntil(handleMessage(e));
});
