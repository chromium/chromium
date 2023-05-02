// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests --gtest_filter=ExtensionWebUITest.OnMessage

if (!chrome || !chrome.test || !chrome.test.onMessage) {
  console.error('chrome.test.onMessage is unavailable on ' +
                document.location.href);
  return false;
}

chrome.test.listenOnce(chrome.test.onMessage, function(args) {
  if (args.data != 'hi') {
    console.error('Expected "hi", Actual ' + JSON.stringify(args.data));
    chrome.test.sendMessage('false');
  } else {
    chrome.test.sendMessage('true');
  }
});

return true;
