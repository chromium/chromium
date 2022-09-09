// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keeps track of who should be receiving keystrokes sent:
// The 'webPage' or the 'backgroundPage'.
var expectedListener = 'webPage';

function gotCommand(command) {
  if (expectedListener == 'backgroundPage') {
    expectedListener = 'webPage';
    chrome.commands.onCommand.removeListener(gotCommand);
    chrome.test.notifyPass();
  } else {
    chrome.test.notifyFail('Webpage expected keystroke, but sent to extension');
  }
}

chrome.extension.onConnect.addListener(function(port) {
  port.onMessage.addListener(function(message) {
    if (expectedListener == 'webPage') {
      expectedListener = 'backgroundPage';
      chrome.commands.onCommand.addListener(gotCommand);
      chrome.test.notifyPass();
    } else {
      chrome.test.notifyFail('Extension expected keystroke, but sent to' +
          ' webpage');
    }
  });
});

chrome.test.notifyPass();
