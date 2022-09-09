// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testPort = chrome.runtime.connect({
  name: 'port from interstitial'
});

var currentTest;
testPort.onMessage.addListener(function(msg) {
  console.assert(!currentTest, 'Should only run one test at a time');
  currentTest = msg;

  if (msg === 'testSendMessage') {
    testSendMessage();
  } else if (msg === 'testDisconnectByBackground') {
    testDisconnectByBackground();
  } else if (msg === 'testDisconnectByInterstitial') {
    testDisconnectByInterstitial();
  } else {
    done('Unexpected test: ' + msg);
  }
});

function done(test) {
  console.assert(test === currentTest, 'test name should match current test');
  currentTest = null;
  testPort.postMessage(test);
}

function testSendMessage() {
  chrome.runtime.sendMessage('First from interstitial', function(msg) {
    chrome.runtime.sendMessage('interstitial received: ' + msg);
    done('testSendMessage');
  });
}

function testDisconnectByBackground() {
  var port = chrome.runtime.connect({
    name: 'disconnect by background'
  });
  port.onDisconnect.addListener(function() {
    done('testDisconnectByBackground');
  });
}

function testDisconnectByInterstitial() {
  var port = chrome.runtime.connect({
    name: 'disconnect by interstitial'
  });
  port.disconnect();
  done('testDisconnectByInterstitial');
}
