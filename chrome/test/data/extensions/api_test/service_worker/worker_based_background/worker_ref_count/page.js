// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FAILURE_MESSAGE = 'FAILURE';

function getServiceWorker() {
  return new Promise(function(resolve, reject) {
    navigator.serviceWorker.ready.then(function (registration) {
      resolve(registration.active);
    });
  });
}

window.testSendMessage = function() {
  getServiceWorker().then(function(serviceWorker) {
    if (serviceWorker == null) {
      chrome.test.sendMessage(FAILURE_MESSAGE);
      return;
    }
    var channel = new MessageChannel();
    channel.port1.onmessage = function(e) {
      if (e.data != 'Worker reply: Hello world') {
        chrome.test.sendMessage(FAILURE_MESSAGE);
      }
    };
    serviceWorker.postMessage('sendMessageTest', [channel.port2]);
  });
};

window.roundtripToWorker = function() {
  return getServiceWorker().then(function(serviceWorker) {
    return new Promise(resolve => {
      if (serviceWorker == null) {
        return resolve('roundtrip-failed');
      }
      var channel = new MessageChannel();
      channel.port1.onmessage = function(e) {
        if (e.data == 'roundtrip-response') {
          return resolve('roundtrip-succeeded');
        }
        return resolve('roundtrip-failed');
      };
      serviceWorker.postMessage('roundtrip-request', [channel.port2]);
    });
  });
};
