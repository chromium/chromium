// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.console.log('Guest script loading.');

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
  if (data[0] == 'create-channel') {
    channelId = data[1];
    notifyEmbedder(['channel-created']);
    return;
  }
  if (data[0] == 'get-style') {
    var propertyName = data[1];
    var computedStyle = window.getComputedStyle(document.body, null);
    var value = computedStyle.getPropertyValue(propertyName);
    notifyEmbedder(['style', propertyName, value]);
    return;
  }
};

window.addEventListener('message', onPostMessageReceived, false);
