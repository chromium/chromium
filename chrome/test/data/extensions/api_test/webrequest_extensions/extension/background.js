// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var baseUrl = 'http://example.com:' + config.testServer.port;

  var x = new XMLHttpRequest();
  x.open('GET', baseUrl + '/extensions/test_file.txt?extension');
  x.onloadend = function() {
    // Just a sanity check to ensure that the server is running.
    // The test does not change the response.
    chrome.test.assertEq('Hello!', x.responseText);

    chrome.test.sendMessage('extension_done');
  };
  x.send();
});
