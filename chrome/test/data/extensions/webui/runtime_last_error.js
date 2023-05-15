// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests --gtest_filter=ExtensionWebUITest.RuntimeLastError

var success = true;

if (!chrome.test.checkDeepEq(undefined, chrome.runtime.lastError)) {
  console.error('Expected undefined, Actual ' +
                JSON.stringify(chrome.runtime.lastError));
  success = false;
}

chrome.test.sendMessage('ping', function(reply) {
  var expected = {
    'message': 'unknown host'
  };
  if (!chrome.test.checkDeepEq(expected, chrome.runtime.lastError)) {
    console.error('Expected ' + JSON.stringify(expected) + ', ' +
                  'Actual ' + JSON.stringify(chrome.runtime.lastError));
    success = false;
  }
  chrome.test.sendMessage(success ? 'true' : 'false');
});

return true;
