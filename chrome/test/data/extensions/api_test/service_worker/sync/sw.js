// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port = null;
this.onsync = function(e) {
  if (port) {
    e.waitUntil(new Promise(function(resolve) {
      // Add a small delay to respond so we can exercise and test the
      // offline->online transition.
      // NOTE: the following setTimeout() is not a requirement for this test. It
      // is here to just demonstrate that the test passes with a bit of
      // asynchrony.
      setTimeout(function() {
        port.postMessage('SYNC: ' + e.tag);
        resolve();
      }, 0);
    }));
  }
};

this.onmessage = function(e) {
  port = e.ports[0];
  if (e.data != 'connect') {
    port.postMessage('SW received unexpected message');
    return;
  }
  port.postMessage('connected');
};
