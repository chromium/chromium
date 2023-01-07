// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
var button1FocusCount = 0;

var LOG = function(msg) {
  window.console.log(msg);
};

var sendMessage = function(msgArray) {
  if (embedder) {
    embedder.postMessage(JSON.stringify(msgArray), '*');
  }
};

var init = function() {
  var button1 = document.createElement('button');
  button1.addEventListener('focus', function(e) {
    LOG('button1.focus');
    ++button1FocusCount;
    sendMessage(['button1-focused', button1FocusCount]);
  });
  button1.innerText = 'Before';
  button1.setAttribute('tabIndex', 0);

  var button2 = document.createElement('button');
  button2.innerText = 'Before';
  button2.setAttribute('tabIndex', 0);

  document.body.appendChild(button1);
  document.body.appendChild(button2);

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    embedder = e.source;
    LOG('message, data: ' + data);
    if (data[0] == 'connect') {
      sendMessage(['connected']);
    }
  });
};

init();
