// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var serviceWorkerPromise = new Promise(function(resolve, reject) {
  navigator.serviceWorker.register('sw.js').then(function() {
    return navigator.serviceWorker.ready;
  }).then(function(registration) {
    var sw = registration.active;
    var channel = new MessageChannel();
    channel.port1.onmessage = function(e) {
      if (e.data == 'Pong from version 1') {
        resolve(e.data);
      } else {
        reject(e.data);  // Fail fast.
      }
    };
    sw.postMessage('ping', [channel.port2]);
  }).catch(function(err) {
    reject(err);
  });
});

serviceWorkerPromise.then(function(message) {
  chrome.test.sendMessage(message);
}).catch(function(err) {
  chrome.test.sendMessage('FAILURE');
});
