// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function postToServiceWorkerAndExpectError(sw) {
  return new Promise(function(resolve, reject) {
    var mc = new MessageChannel();
    mc.port1.onmessage = function(e) {
      mc.port1.onmessage = null;
      if (e.data.success)
        reject('getBackgroundClient was successful, it should not have been');
      else if (e.data.error != 'Failed to start background client ""')
        reject('getBackgroundClient gave unexpected error ' + e.data.error);
      else
        resolve();
    };
    sw.postMessage({port: mc.port2}, [mc.port2]);
  });
}

function test() {
  // Just testing that register succeeds is enough. The background page tests
  // exercise the service worker fetch and messaging logic.
  navigator.serviceWorker.register('sw.js').then(function() {
    // Wait until the service worker is active.
    return navigator.serviceWorker.ready;
  }).then(function(r) {
    // Ask the SW a bunch of times, it should fail each time. The order doesn't
    // matter, it's idempotent (it should always fail).
    return Promise.all([postToServiceWorkerAndExpectError(r.active),
                        postToServiceWorkerAndExpectError(r.active),
                        postToServiceWorkerAndExpectError(r.active),
                        postToServiceWorkerAndExpectError(r.active)]);
  }).then(function() {
    chrome.test.succeed();
  }).catch(function(err) {
    chrome.test.fail(err.message);
  });
}

chrome.test.runTests([test]);
