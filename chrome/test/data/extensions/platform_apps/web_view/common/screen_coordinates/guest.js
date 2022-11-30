// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The window reference of the embedder to send post message reply.
var embedderWindowChannel = null;

// A value that uniquely identifies the guest sending the messages to the
// embedder.
var channelId = 0;
var notifyEmbedder = function(msg_array) {
  var msg = msg_array.concat([channelId]);
  embedderWindowChannel.postMessage(JSON.stringify(msg), '*');
};

// Notifies the embedder about the result of the request (success/fail)
// via post message. Note that the embedder has to initiate a postMessage
// first so that guest has a reference to the embedder's window.
var onPostMessageReceived = function(e) {
  var data = JSON.parse(e.data);
  window.console.log('Guest: onPostMessageReceived, data[0] = ' + data[0]);
  if (data[0] == 'create-channel') {
    embedderWindowChannel = e.source;
    window.console.log('guest: create-channel');
    channelId = data[1];
    notifyEmbedder(['channel-created']);
    return;
  } else if (data[0] == 'test1') {
    var screenInfo = {
      'screenX': window.screenX,
      'screenY': window.screenY,
      'screenLeft': window.screenLeft,
      'screenTop': window.screenTop
    };
    var responseArray = [];
    responseArray.push(data[0]);
    responseArray.push(screenInfo);
    notifyEmbedder(responseArray);
  }
};

window.addEventListener('message', onPostMessageReceived, false);
window.console.log('guest load complete');
