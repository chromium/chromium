// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests
//     --gtest_filter=ExtensionWebUITest.ReceivesExtensionOptionsOnClose
if (!chrome || !chrome.test || !chrome.test.sendMessage) {
  console.error('chrome.test.sendMessage is unavailable on ' +
                document.location.href);
  return false;
}

chrome.test.sendMessage('ready', function(reply) {
  var extensionoptions = document.createElement('extensionoptions');
  extensionoptions.onclose = function() {
    chrome.test.sendMessage('onclose received');
  };
  extensionoptions.setAttribute('extension', reply);
  document.body.appendChild(extensionoptions);
});

return true;
