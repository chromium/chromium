// Copyright 2014 The Chromium Authors
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

  switch (data[0]) {
    case 'create-channel':
      channelId = data[1];
      notifyEmbedder(['channel-created']);
      break;
    case 'create-frame':
      window.console.log('command from embedder: create-frame');
      // In the guest.
      var div = document.createElement('div');
      div.id = 'testDiv';
      div.innerText = 'guest:';
      document.body.appendChild(div);

      // Create 'testDiv' element inside the iframe.
      var iframe = document.createElement('iframe');
      iframe.onload = function() {
        window.console.log('onload fire');
        var doc = iframe.contentDocument;
        var idiv = doc.createElement('div');
        idiv.id = 'testDiv';
        idiv.innerText = 'frame:';
        doc.body.appendChild(idiv);

        notifyEmbedder(['created-frame']);
      };
      iframe.src = 'about:blank';  // Matches guest's origin.
      document.body.appendChild(iframe);
      break;
    case 'get-testDiv-innerText':
      var iframe = document.querySelector('iframe');
      var divContent =
          document.getElementById('testDiv').innerText;
      var iframeContent =
          iframe.contentDocument.getElementById('testDiv').innerText;
      notifyEmbedder(['got-testDiv-innerText', divContent, iframeContent]);
      break;
    default:
      // We're not expected to get here, fail fast by sending a 'bogus' message
      // to the embedder.
      notifyEmbedder(['bogus']);
      break;
  }
};

window.addEventListener('message', onPostMessageReceived, false);
