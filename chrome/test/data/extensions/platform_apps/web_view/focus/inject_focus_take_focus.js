// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;

var LOG = function(msg) {
  window.console.log(msg);
};

var sendMessage = function(msgArray) {
  if (embedder) {
    embedder.postMessage(JSON.stringify(msgArray), '*');
  }
};

var init = function() {
  var input = document.createElement('input');
  input.style.width = '100%';
  input.addEventListener('focus', function(e) {
    sendMessage(['focused']);
  });

  document.body.appendChild(input);

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    embedder = e.source;
    LOG('message, data: ' + data);
    if (data[0] == 'connect') {
      sendMessage(['connected']);
    } else if (data[0] == 'request-coords') {
      var rect = input.getBoundingClientRect();
      sendMessage(['response-coords',
          rect.left + 0.5 * rect.width,
          rect.top + 0.5 * rect.height]);
    }
  });
};

init();
