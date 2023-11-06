// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For complex connect tests.
chrome.runtime.onConnect.addListener(function onConnect(port) {
  console.log('connected');
  port.onMessage.addListener(function(msg) {
    console.log('got ' + JSON.stringify(msg));
    if (msg.testPostMessage) {
      port.postMessage({success: true});
    } else if (msg.testPostMessageFromTab) {
      testPostMessageFromTab(port);
    } else if (msg.testSendMessageFromTab) {
      testSendMessageFromTab();
    } else if (msg.testSendMessageFromFrame) {
      testSendMessageFromFrame();
    } else if (msg.testSendMessageFromSandboxedFrame) {
      testSendMessageFromSandboxedFrame();
    } else if (msg.testSendMessageToFrame) {
      port.postMessage('from_main');
    } else if (msg.testDisconnect) {
      port.disconnect();
    } else if (msg.testConnectChildFrameAndNavigateSetup) {
      chrome.runtime.onConnect.removeListener(onConnect);
      chrome.test.assertFalse(chrome.runtime.onConnect.hasListeners());
      testConnectChildFrameAndNavigateSetup();
    } else if (msg.testNavigateAwayAndHistoryBack) {
      // This tests the following scenario:
      // 1. Open port to the background script (test.js).
      // 2. The background script (test.js) sends a message
      //    ("navigateAwayAndHistoryBack") to this page.
      // 3. Attach a pageshow event handler on this page.
      // 4. Navigate to history_back.html, and navigate away from the
      //    current page.
      // 5. The current page is stored in the back/forward cache.
      // 6. history_back.html runs window.history.back().
      // 7. The previous page is salvaged from the back/forward cache.
      // 8. The pageshow event handler sends two messages to the
      //    background script (test.js) to confirm that these ports
      //    that are opened before navigating away are still
      //    available.
      let portFromTab = chrome.runtime.connect();
      portFromTab.onMessage.addListener(function(msg) {
        window.addEventListener('pageshow', function(event) {
          if (event.persisted) {
            port.postMessage({salvagedFromBackForwardCache1: true});
            portFromTab.postMessage({salvagedFromBackForwardCache2: true});
          }
        });
        chrome.test.assertEq('navigateAwayAndHistoryBack', msg);
        window.location = 'api_test/messaging/connect/history_back.html';
      });
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
  // frame.js sends a message to the background page, which checks that the
  // sender has the expected frameId, url, origin, tab and runtime id.
  for (var i = 0; i < 2; ++i) {
    var f = document.createElement('iframe');
    f.src = '?testSendMessageFromFrame' + i;
    document.body.appendChild(f);
  }
}

function testSendMessageFromSandboxedFrame() {
  // Add two frames. The content script declared in manifest.json (frame.js)
  // runs in frames whose URL matches ?testSendMessageFromSandboxFrame.
  // frame.js sends a message to the background page, which checks that the
  // sender has the expected frameId, url, origin, tab and runtime id.
  for (var i = 2; i < 4; ++i) {
    var f = document.createElement('iframe');
    f.sandbox = 'allow-scripts';
    f.src = '?testSendMessageFromSandboxedFrame' + i;
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
  const extensionOrigin = new URL(chrome.runtime.getURL('')).origin;
  chrome.test.assertEq(
      { id: chrome.runtime.id, origin: extensionOrigin }, sender);
  sendResponse({success: (request.step2 == 1)});
});
