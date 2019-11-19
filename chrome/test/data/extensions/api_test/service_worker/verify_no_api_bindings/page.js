// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var worker = null;
var FAILURE_MESSAGE = 'FAILURE';

window.runServiceWorker = function() {
  navigator.serviceWorker.register('sw.js').then(function() {
    return navigator.serviceWorker.ready;
  }).then(function(registration) {
    worker = registration.active;
    chrome.test.sendMessage('WORKER STARTED');
  }).catch(function(err) {
    chrome.test.sendMessage(FAILURE_MESSAGE);
  });
};

window.testSendMessage = function() {
  if (worker == null) {
    chrome.test.sendMessage(FAILURE_MESSAGE);
    return;
  }
  var channel = new MessageChannel();
  channel.port1.onmessage = function(e) {
    chrome.test.sendMessage(e.data);
  };
  worker.postMessage('checkBindingsTest', [channel.port2]);
};
