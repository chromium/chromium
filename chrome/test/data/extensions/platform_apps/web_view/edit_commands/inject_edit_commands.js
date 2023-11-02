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

  if (data[0] == 'select-all') {
    var range = document.createRange();
    range.selectNode(document.body);
    window.getSelection().addRange(range);
    var msg = ['selected-all'];
    embedder.postMessage(JSON.stringify(msg), '*');
    return;
  }
});

window.addEventListener('copy', function(e) {
  var msg = ['copy'];
  embedder.postMessage(JSON.stringify(msg), '*');
});
