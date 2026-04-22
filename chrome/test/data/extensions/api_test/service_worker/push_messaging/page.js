// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const startServiceWorker = function() {
  return new Promise(function(resolve, reject) {
    navigator.serviceWorker.register('sw.js')
        .then(function() {
          // Wait until the service worker is active.
          return navigator.serviceWorker.ready;
        })
        .then(function(registration) {
          const serviceWorker = registration.active;
          registration.pushManager
              .subscribe({
                userVisibleOnly: true,
              })
              .then(function(subscription) {
                resolve(serviceWorker);
              })
              .catch(function(err) {
                reject(`pushManager.subscription: ${err}`);
              });
        })
        .catch(function(err) {
          reject(err);
        });
  });
};

window.runServiceWorker = function() {
  const sendMessage = function(str, optExtraLogStr) {
    console.log(str);
    if (optExtraLogStr) {
      console.log(optExtraLogStr);
    }
    chrome.test.sendMessage(str);
  };

  startServiceWorker()
      .then(function(serviceWorker) {
        const mc = new MessageChannel();
        serviceWorker.postMessage('waitForPushMessaging', [mc.port2]);
        mc.port1.onmessage = function(e) {
          sendMessage(
              e.data == 'testdata' ? 'OK' : 'FAIL', `message: ${e.data}`);
        };
        sendMessage('SERVICE_WORKER_READY');
      })
      .catch(function(err) {
        sendMessage('SERVICE_WORKER_FAILURE', err);
      });
};
