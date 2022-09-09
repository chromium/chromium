// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) {
  window.console.log(msg);
};

var touchDiv;
var embedder;

function init() {
  touchDiv = document.createElement('div');
  touchDiv.innerText = 'With touch';
  document.body.appendChild(touchDiv);
}

function handler() {}

function onAppCommand(command) {
  LOG('onAppCommand, command = ' + command);
  switch (command) {
    case 'install-touch-handler':
      touchDiv.addEventListener('touchstart', handler);
      sendMessageToEmbedder('installed-touch-handler');
      break;
    case 'uninstall-touch-handler':
      touchDiv.removeEventListener('touchstart', handler);
      sendMessageToEmbedder('uninstalled-touch-handler');
      break;
  }
};

function sendMessageToEmbedder(message) {
  if (!embedder) {
     LOG('no embedder channel to send postMessage');
     return;
  }

  embedder.postMessage(JSON.stringify([message]), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    sendMessageToEmbedder('connected');
  }
});

window.onload = init;
