// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var javaScriptURL = 'javascript:void(document.title=%22js-url-success%22)';

  var url = 'http://a.com:' + config.testServer.port +
            '/extensions/test_file.html';

  chrome.tabs.create({ url: url }, function(tab) {
    var tab_id = tab.id;

    chrome.test.runTests([
      function javascript_encoded_url() {
        chrome.tabs.update(
          tab_id,
          {url: javaScriptURL},
          function(tab) {
            chrome.test.assertEq('js-url-success', tab.title);
            chrome.test.succeed();
          }
        );
      }
    ]);
  });
});
