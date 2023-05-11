// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This is a common file for all service worker tests, which registers a
// service worker on startup, and provides testing utilities to be run from
// ServiceWorkerTest (service_worker_apitest.cc).

// Namespace for all testing utilities and state.
var test = {
  // The service worker that was registered.
  registeredServiceWorker: null,
  // The last message's data fired on window.onmessage.
  lastMessageFromServiceWorker: null,
};

// Registers a service worker and stores it in registeredServiceWorker.
// Intended to be called from content::ExecJs.
test.registerServiceWorker = function(path) {
  return navigator.serviceWorker.register(path).then(function() {
    // Wait until the service worker is active.
    return navigator.serviceWorker.ready;
  }).then(function(r) {
    test.registeredServiceWorker = r.active;
    return '';
  }).catch(function(err) {
    return err.message;
  });
};

// Watch for messages.
navigator.serviceWorker.addEventListener('message', function(e) {
  test.lastMessageFromServiceWorker = e.data;
  // Echo message if requested.
  if (e.data.echo)
    e.data.echo.port.postMessage(e.data.echo.message);
});

// Returns a Promise for the next message's data sent to |endpoint|.
test.waitForMessage = function(endpoint) {
  return new Promise(function(resolve) {
    endpoint.onmessage = function listener(e) {
      endpoint.onmessage = null;
      resolve(e.data);
    };
  });
};

chrome.runtime.onMessage.addListener(function(msg, _, sendResponse) {
  sendResponse({label: 'onMessage/original BG.'});
});

chrome.test.sendMessage('ready');
