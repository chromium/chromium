// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) { window.console.log(msg); };
LOG('Guest script loading.');

// The window reference of the embedder to send post message reply.
var embedderWindowChannel = null;

// A value that uniquely identifies the guest sending the messages to the
// embedder.
var channelId = 0;
var notifyEmbedder = function(msg_array) {
  var msg = msg_array.concat([channelId]);
  embedderWindowChannel.postMessage(JSON.stringify(msg), '*');
};

var onPostMessageReceived = function(e) {
  embedderWindowChannel = e.source;
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    channelId = data[1];
    notifyEmbedder(['connected']);
    return;
  }
};

var setup = function() {
  // Make sure there is always stuff to show in context menu by
  // inserting an <input> element.
  // Note that we don't always show "Inspect element", so this is
  // necessary.
  var div = document.createElement('div');
  div.style.position = 'absolute';
  div.style.top = 0;
  div.style.left = 0;
  var input = document.createElement('input');
  div.appendChild(input);
  document.body.style.padding = 0;
  document.body.style.margin = 0;
  document.body.appendChild(div);
};

setup();
window.addEventListener('message', onPostMessageReceived, false);
