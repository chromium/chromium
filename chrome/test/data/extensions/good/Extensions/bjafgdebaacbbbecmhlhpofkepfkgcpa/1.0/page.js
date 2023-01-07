// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var win = window;
if (typeof(contentWindow) != 'undefined') {
  win = contentWindow;
}

chrome.runtime.onConnect.addListener(function(port) {
  console.log('connected');
  port.onMessage.addListener(function(msg) {
    console.log('got ' + msg);
    if (msg.testPostMessage) {
      port.postMessage({success: true});
    } else if (msg.testPostMessageFromTab) {
      testPostMessageFromTab(port);
    } else if (msg.testDisconnect) {
      port.disconnect();
    } else if (msg.testDisconnectOnClose) {
      win.location = "about:blank";
    } else if (msg.testPortName) {
      port.postMessage({portName:port.name});
    }
    // Ignore other messages since they are from us.
  });
});

// Tests that postMessage to the extension and its response works.
function testPostMessageFromTab(origPort) {
  console.log('testPostMessageFromTab');
  var portName = "peter";
  var port = chrome.runtime.connect({name: portName});
  port.postMessage({testPostMessageFromTab: true});
  port.onMessage.addListener(function(msg) {
    origPort.postMessage({success: (msg.success && (msg.portName == portName))});
    console.log('sent ' + msg.success);
    port.disconnect();
  });
  console.log('posted message');
}

// Workaround two bugs: shutdown crash if we hook 'unload', and content script
// GC if we don't register any event handlers.
// http://code.google.com/p/chromium/issues/detail?id=17410
// http://code.google.com/p/chromium/issues/detail?id=17582
function foo() {}
win.addEventListener('error', foo);
