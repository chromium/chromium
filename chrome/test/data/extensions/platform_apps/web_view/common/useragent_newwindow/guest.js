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

var onPostMessageReceived;
if (!onPostMessageReceived) {
  onPostMessageReceived = function(e) {
    embedderWindowChannel = e.source;
    var data = JSON.parse(e.data);
    if (data[0] == 'create-channel') {
      channelId = data[1];
      notifyEmbedder(['channel-created']);
      return;
    }

    // Tests.
    // These logs trigger event listeners in the embedder.
    switch (data[0]) {
      case 'get-user-agent':
        notifyEmbedder(['got-user-agent', navigator.userAgent]);
        break;
      case 'open-window': {
        var url = data[1];
        window.open(url);
        break;
      }
      default:
        break;
    }
  };
  window.addEventListener('message', onPostMessageReceived, false);
}
