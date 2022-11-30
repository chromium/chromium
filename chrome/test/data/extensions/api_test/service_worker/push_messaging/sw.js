// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port;
var pushData;

function maybeFinish() {
  if (port && pushData) {
    port.postMessage(pushData);
  }
}

this.onpush = function(e) {
  pushData = e.data.text();
  maybeFinish();
};

this.onmessage = function(e) {
  if (e.data == 'waitForPushMessaging') {
    port = e.ports[0];
    maybeFinish();
  }
};
