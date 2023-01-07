// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
var LOG = function(msg) { window.console.log(msg); };

window.addEventListener('message', function(e) {
  LOG('message');
  embedder = e.source;
  var data = JSON.parse(e.data);
  LOG('data: ' + data);
  switch (data[0]) {
    case 'dimension-request':
      var reply = ['dimension-response', window.innerWidth, window.innerHeight];
      embedder.postMessage(JSON.stringify(reply), '*');
      break;
  }
});
