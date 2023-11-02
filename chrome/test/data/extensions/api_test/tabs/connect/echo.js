// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content script that echoes back all messages.
// Posting a message with "GET" returns the name and # of connections opened.
var connections = 0;

// Notify the test that the content script is ready to go.
chrome.runtime.sendMessage('ready');

chrome.runtime.onConnect.addListener(function onConnect(port) {
  connections++;
  port.onMessage.addListener(function onMessage(msg) {
    if (msg == "GET") {
      port.postMessage({"name": port.name, "connections": connections});
    } else {
      port.postMessage(msg);
    }
  });
});

// onRequest simply echoes everything.
chrome.extension.onRequest.addListener(function(request, sender, respond) {
  respond(request);
});

// onMessage accepts commands (not all of which relate to echoing).
chrome.runtime.onMessage.addListener(function(request, sender, respond) {
  if (request.open)
    open(request.open);
  if (request.send)
    chrome.runtime.sendMessage(request.send);
  respond();
});
