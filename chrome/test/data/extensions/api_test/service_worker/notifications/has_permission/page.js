// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var startServiceWorker = function() {
  return new Promise(function(resolve, reject) {
    navigator.serviceWorker.register('sw.js').then(function() {
      // Wait until the service worker is active.
      return navigator.serviceWorker.ready;
    }).then(function(registration) {
      resolve(registration.active);
    }).catch(function(err) {
      reject(err);
    });
  });
};

var test = function(messageToSend) {
  return new Promise(function(resolve, reject) {
    startServiceWorker().then(function(serviceWorker) {
      var mc = new MessageChannel();
      mc.port1.onmessage = function(e) {
        if (e.data == 'OK') {
          resolve();
        } else {
          reject('Received bad response from ServiceWorker: ' + e.data);
        }
      };
      serviceWorker.postMessage(messageToSend, [mc.port2]);
    }).catch(function(err) {
      reject('Failed to start ServiceWorker, error: ' + err);
    });
  });
};

chrome.test.runTests([
  function checkNotification() {
    test('checknotification').then(chrome.test.notifyPass,
                                   chrome.test.notifyFail);
  },
  function showNotification() {
    test('shownotification').then(chrome.test.notifyPass,
                                  chrome.test.notifyFail);
  }
]);
