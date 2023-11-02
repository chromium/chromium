// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var controllerChangePromise = new Promise(function(resolve, reject) {
  navigator.serviceWorker.oncontrollerchange = function(e) {
    navigator.serviceWorker.ready.then(function(registration) {
      resolve(registration.active);
    }).catch(function(err) {
      reject('oncontrollerchange failure');
    });
  };
});

controllerChangePromise.then(function(serviceWorker) {
  var channel = new MessageChannel();
  channel.port1.onmessage = function(e) {
    chrome.test.log('Message received from SW: ' + e.data);
    chrome.test.sendMessage(e.data);
  };
  serviceWorker.postMessage('ping', [channel.port2]);
}).catch(function(err) {
  console.log(err);
  chrome.test.sendMessage('FAILURE_V2');
});
