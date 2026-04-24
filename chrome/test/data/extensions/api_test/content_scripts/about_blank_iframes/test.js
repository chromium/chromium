// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkFirstMessageEquals(expectedRequest) {
  return function(request) {
    if (request != expectedRequest) {
      chrome.test.fail('Unexpected request: ' + JSON.stringify(request));
    }
    // chrome.test.succeed() will be called by chrome.test.listenOnce().
    // If this function is not used by chrome.test.listenOnce(), then
    // call chrome.test.succeed() when you're done.
  };
}

const onMessage = chrome.runtime.onMessage;
chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function testDontInjectInAboutBlankFrame() {
      chrome.test.listenOnce(onMessage, checkFirstMessageEquals('parent'));
      chrome.test.log('Creating tab...');
      const testUrl = `http://localhost:${config.testServer.port}` +
          '/extensions/test_file_with_about_blank_iframe.html';
      chrome.tabs.create({url: testUrl});
    },
    function testDontInjectInAboutSrcdocFrame() {
      chrome.test.listenOnce(onMessage, checkFirstMessageEquals('parent'));
      chrome.test.log('Creating tab...');
      const testUrl = `http://localhost:${config.testServer.port}` +
          '/extensions/api_test/webnavigation/srcdoc/a.html';
      chrome.tabs.create({url: testUrl});
    },
    function testDontInjectInNestedAboutFrames() {
      chrome.test.listenOnce(onMessage, checkFirstMessageEquals('parent'));
      chrome.test.log('Creating tab...');
      const testUrl = `http://localhost:${config.testServer.port}` +
          '/extensions/test_file_with_about_blank_in_srcdoc.html';
      chrome.tabs.create({url: testUrl});
    },
    function testDocumentStartRunsInSameWorldAsDocumentEndOfJavaScriptUrl() {
      onMessage.addListener(function listener(request) {
        onMessage.removeListener(listener);
        // The empty document was replaced with the result of the evaluated
        // JavaScript code.
        checkFirstMessageEquals('jsresult/something')(request);
        chrome.test.succeed();
      });
      chrome.test.log('Creating tab...');
      const testUrl = `http://localhost:${config.testServer.port}` +
          '/extensions/test_file_with_javascript_url_iframe.html';
      chrome.tabs.create({url: testUrl});
    },
  ]);
});
