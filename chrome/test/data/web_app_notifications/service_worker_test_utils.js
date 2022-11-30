// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Posts a message to service worker and awaits the response.
function postRequestAwaitResponse(message) {
  return new Promise((result, reject) => {
    const channel = new MessageChannel();

    channel.port1.onmessage = ({data}) => {
      channel.port1.close();
      if (data.error) {
        reject(data.error);
      } else {
        result(data.result);
      }
    };

    return navigator.serviceWorker.controller.postMessage(
        message, [channel.port2]);
  });
}

async function awaitServiceWorkerActivation() {
  let registration = await navigator.serviceWorker.ready;
  let activeWorker = null;

  if (registration && registration.active)
    activeWorker = registration.active;
  else
    throw new Error('Service Worker is in wrong state.');

  if (activeWorker.state === 'activated')
    return true;

  return new Promise(function(resolve, reject) {
    activeWorker.addEventListener('statechange', function() {
      if (activeWorker.state === 'activated')
        resolve(true);
      else
        reject('Service Worker is in wrong state.');
    });
  });
}
