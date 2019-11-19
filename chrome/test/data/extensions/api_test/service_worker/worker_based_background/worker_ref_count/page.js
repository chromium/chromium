// Copyright 2019 The Chromium Authors. All rights reserved.
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
  getServiceWorker().then(function(serviceWorker) {
    if (serviceWorker == null) {
      window.domAutomationController.send('roundtrip-failed');
    }
    var channel = new MessageChannel();
    channel.port1.onmessage = function(e) {
      if (e.data == 'roundtrip-response') {
        window.domAutomationController.send('roundtrip-succeeded');
      } else {
        window.domAutomationController.send('roundtrip-failed');
      }
    };
    serviceWorker.postMessage('roundtrip-request', [channel.port2]);
  });
};
