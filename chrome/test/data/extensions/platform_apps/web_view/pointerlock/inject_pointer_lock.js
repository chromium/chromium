// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    embedder = e.source;
    var msg = ['connected'];
    embedder.postMessage(JSON.stringify(msg), '*');
    return;
  }

  if (data == 'start-pointerlock') {
    document.body.requestPointerLock();
    return;
  }
});

document.addEventListener('pointerlockchange', function(e) {
  if (!!document.pointerLockElement) {
    var msg = ['acquired-pointerlock'];
    embedder.postMessage(JSON.stringify(msg), '*');
    return;
  }
  var msg = ['lost-pointerlock'];
  embedder.postMessage(JSON.stringify(msg), '*');
});
