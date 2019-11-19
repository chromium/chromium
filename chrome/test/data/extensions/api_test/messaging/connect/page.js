// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For complex connect tests.
chrome.runtime.onConnect.addListener(function onConnect(port) {
  console.log('connected');
  port.onMessage.addListener(function(msg) {
    console.log('got ' + msg);
    if (msg.testPostMessage) {
      port.postMessage({success: true});
    } else if (msg.testPostMessageFromTab) {
      testPostMessageFromTab(port);
    } else if (msg.testSendMessageFromTab) {
      testSendMessageFromTab();
    } else if (msg.testSendMessageFromFrame) {
      testSendMessageFromFrame();
    } else if (msg.testSendMessageToFrame) {
      port.postMessage('from_main');
    } else if (msg.testDisconnect) {
      port.disconnect();
    } else if (msg.testConnectChildFrameAndNavigateSetup) {
      chrome.runtime.onConnect.removeListener(onConnect);
      chrome.test.assertFalse(chrome.runtime.onConnect.hasListeners());
      testConnectChildFrameAndNavigateSetup();
    } else if (msg.testDisconnectOnClose) {
      chrome.runtime.connect().onMessage.addListener(function(msg) {
        chrome.test.assertEq('unloadTabContent', msg);
        window.location = 'about:blank';
      });
    } else if (msg.testPortName) {
      port.postMessage({portName:port.name});
    } else if (msg.testSendMessageFromTabError) {
      testSendMessageFromTabError();
    } else if (msg.testConnectFromTabError) {
      testConnectFromTabError();
    }
  });
});

// Tests that postMessage to the extension and its response works.
function testPostMessageFromTab(origPort) {
  var portName = "peter";
  var port = chrome.runtime.connect({name: portName});
  port.postMessage({testPostMessageFromTab: true});
  port.onMessage.addListener(function(msg) {
    origPort.postMessage({success: (msg.success && (msg.portName == portName))});
    console.log('testPostMessageFromTab sent ' + msg.success);
    port.disconnect();
  });
}

// For test onMessage.
function testSendMessageFromTab() {
  chrome.runtime.sendMessage({step: 1}, function(response) {
    if (response.nextStep) {
      console.log('testSendMessageFromTab sent');
      chrome.runtime.sendMessage({step: 2});
    }
  });
}

function testSendMessageFromFrame() {
  // Add two frames. The content script declared in manifest.json (frame.js)
  // runs in frames whose URL matches ?testSendMessageFromFrame.
  // frame.js sends a message to the background page, which checks that
  // sender.frameId exists and is different for both frames.
  for (var i = 0; i < 2; ++i) {
    var f = document.createElement('iframe');
    f.src = '?testSendMessageFromFrame' + i;
    document.body.appendChild(f);
  }
}

function testConnectChildFrameAndNavigateSetup() {
  var frames = document.querySelectorAll('iframe');
  for (var i = 0; i < frames.length; ++i) {
    frames[i].remove();
  }
  var f = document.createElement('iframe');
  f.src = '?testConnectChildFrameAndNavigateSetup';
  document.body.appendChild(f);
  // Test will continue in frame.js
}

// Use a potentially-valid extension id.
var fakeExtensionId = 'c'.repeat(32);

// Tests sendMessage to an invalid extension.
function testSendMessageFromTabError() {
  // try sending a request to a bad extension id
  chrome.runtime.sendMessage(fakeExtensionId, {m: 1}, function(response) {
    var success = (response === undefined && chrome.runtime.lastError);
    chrome.runtime.sendMessage({success: success});
  });
}

// Tests connecting to an invalid extension.
function testConnectFromTabError() {
  var port = chrome.runtime.connect(fakeExtensionId);
  port.onDisconnect.addListener(function() {
    var success = (chrome.runtime.lastError ? true : false);
    chrome.runtime.sendMessage({success: success});
  });
}

// For test sendMessage.
chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  chrome.test.assertEq({id: chrome.runtime.id}, sender);
  sendResponse({success: (request.step2 == 1)});
});
