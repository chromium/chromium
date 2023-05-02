// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests --gtest_filter=ExtensionWebUITest.SendMessage

if (!chrome || !chrome.test || !chrome.test.sendMessage) {
  console.error('chrome.test.sendMessage is unavailable on ' +
                document.location.href);
  return false;
}

chrome.test.sendMessage('ping', function(reply) {
  if (reply != 'pong') {
    console.error('Expected "pong", Actual ' + JSON.stringify(reply));
    chrome.test.sendMessage('false');
  } else {
    chrome.test.sendMessage('true');
  }
});

return true;
