// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This port is opened by a content script in an interstitial. This is used by
// the tests to drive the tests.
var testPort;
var testPortPromise = new Promise(function(resolve) {
  chrome.test.listenOnce(chrome.runtime.onConnect, function(port) {
    testPort = port;
    resolve();
  });
});

// Start a test and wait until the set-up (not necessarily the test!) is done.
function sendToInterstitialAndWait(testName) {
  testPort.postMessage(testName);
  chrome.test.listenOnce(testPort.onMessage, function(msg) {
    chrome.test.assertEq(testName, msg);
  });
}

function assertIsPortFromInterstitial(port, expectedName) {
  chrome.test.assertEq(expectedName, port.name);
  chrome.test.assertEq(undefined, port.sender.tab);
  chrome.test.assertEq(undefined, port.sender.frameId);
  chrome.test.assertTrue(port.sender.url.startsWith('data:'));
}

var httpsTabId;
var httpsTabIdPromise = new Promise(function(resolve) {
  // The test runner will open a https page after loading the extension.
  chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
    if (tab.url.startsWith('https://')) {
      chrome.tabs.onUpdated.removeListener(listener);
      httpsTabId = tabId;
      resolve();
    }
  });
});

chrome.test.runTests([
  function waitForHttpsPage() {
    httpsTabIdPromise.then(chrome.test.callbackPass(function() {
      chrome.tabs.query({
        url: 'https://*/*'
      }, chrome.test.callbackPass(function(tabs) {
        // Sanity check. There should only be one https tab.
        chrome.test.assertEq(1, tabs.length);
      }));
    }));
  },

  // All following tests rely on testPort, so ensure that it exists!
  function waitForPortFromInterstitial() {
    testPortPromise.then(chrome.test.callbackPass(function() {
      assertIsPortFromInterstitial(testPort, 'port from interstitial');
    }));
  },

  // Tests whether ping-ponging with sendMessage works.
  function testSendMessage() {
    chrome.test.listenOnce(chrome.runtime.onMessage,
        function(msg, sender, sendResponse) {
          chrome.test.assertEq('First from interstitial', msg);

          var kResponse = 'hello me!';
          chrome.test.listenOnce(chrome.runtime.onMessage, function(msg) {
            chrome.test.assertEq('interstitial received: ' + kResponse, msg);
          });
          sendResponse(kResponse);
        });
    sendToInterstitialAndWait('testSendMessage');
  },

  // Tests whether the onDisconnect event is fired in the interstitial page.
  function testDisconnectByBackground() {
    chrome.test.listenOnce(chrome.runtime.onConnect,
        function(port) {
          assertIsPortFromInterstitial(port, 'disconnect by background');
          port.disconnect();
        });
    sendToInterstitialAndWait('testDisconnectByBackground');
  },

  // Tests whether the onDisconnect event is fired when the port is closed from
  // the content script in the interstitial page.
  function testDisconnectByInterstitial() {
    chrome.test.listenOnce(chrome.runtime.onConnect,
        function(port) {
          assertIsPortFromInterstitial(port, 'disconnect by interstitial');
          chrome.test.listenOnce(port.onDisconnect, function() {
            chrome.test.assertNoLastError();
          });
        });
    sendToInterstitialAndWait('testDisconnectByInterstitial');
  },

  // Closing the interstitial should cause the ports to disconnect.
  function testDisconnectByClosingInterstitial() {
    chrome.test.listenOnce(testPort.onDisconnect, function() {
      chrome.test.assertNoLastError();
      testPort = null;
    });
    // Close the interstitial. Should trigger onDisconnect.
    chrome.tabs.update(httpsTabId, {
      url: 'about:blank'
    });
  },
]);
