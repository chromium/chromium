// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) { window.console.log(msg); };
LOG('Guest script loading.');

var fail = function() {
  // Embedder catches this message and fails the test.
  LOG('ERROR');
};

var sendXhr = function() {
  var xhr = new XMLHttpRequest();
  xhr.onload = function() {
    LOG('xhr.onload');
    if (xhr.responseText != 'dummy text') {
      fail();
    }
  };
  xhr.onerror = function() {
    fail();
  };
  xhr.open('GET', '/cache-control-response', true);
  xhr.send();
};

var onPostMessageReceived = function(e) {
  var data = JSON.parse(e.data);
  if (data[0] != 'sendXhr') {
    fail();
    return;
  }

  sendXhr();
};
window.addEventListener('message', onPostMessageReceived, false);
