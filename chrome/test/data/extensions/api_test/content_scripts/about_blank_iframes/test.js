// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkFirstMessageEquals(expectedRequest) {
  return function(request) {
    if (request != expectedRequest)
      chrome.test.fail('Unexpected request: ' + JSON.stringify(request));
    // chrome.test.succeed() will be called by chrome.test.listenOnce().
    // If this function is not used by chrome.test.listenOnce(), then
    // call chrome.test.succeed() when you're done.
  };
}

var onRequest = chrome.extension.onRequest;
chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function testDontInjectInAboutBlankFrame() {
      chrome.test.listenOnce(onRequest, checkFirstMessageEquals('parent'));
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'test_file_with_about_blank_iframe.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    function testDontInjectInAboutSrcdocFrame() {
      chrome.test.listenOnce(onRequest, checkFirstMessageEquals('parent'));
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'api_test/webnavigation/srcdoc/a.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    function testDontInjectInNestedAboutFrames() {
      chrome.test.listenOnce(onRequest, checkFirstMessageEquals('parent'));
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'test_file_with_about_blank_in_srcdoc.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    function testDocumentStartRunsInSameWorldAsDocumentEndOfJavaScriptUrl() {
      onRequest.addListener(function listener(request) {
        onRequest.removeListener(listener);
        // The empty document was replaced with the result of the evaluated
        // JavaScript code.
        checkFirstMessageEquals('jsresult/something')(request);
        chrome.test.succeed();
      });
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'test_file_with_javascript_url_iframe.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    }
  ]);
});
