// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var testUrl = 'http://b.com:' + config.testServer.port +
                '/extensions/api_test/executescript/css_origin/test.html';
  chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    chrome.tabs.onUpdated.removeListener(listener);
    chrome.test.runTests([
      // Until we have tabs.removeCSS we just have to target a different
      // element on the page for each test.
      function authorOriginShouldSucceed() {
        var injectDetails = {};
        injectDetails.code = '#author {' +
                             ' color: blue !important;' +
                             ' background-color: white !important;' +
                             '}';
        injectDetails.cssOrigin = 'author';
        chrome.tabs.insertCSS(tabId, injectDetails,
                              chrome.test.callbackPass(function() {
          chrome.tabs.sendMessage(tabId, { id: 'author' },
                                  chrome.test.callbackPass(function(response) {
            chrome.test.assertEq('rgb(0, 0, 255)', response.color);
            // !important rules in author style sheets do not override inline
            // !important rules.
            chrome.test.assertEq('rgb(0, 0, 0)', response.backgroundColor);
          }));
        }));
      },
      function userOriginShouldSucceed() {
        var injectDetails = {};
        injectDetails.code = '#user {' +
                             ' color: blue !important;' +
                             ' background-color: white !important;' +
                             '}';
        injectDetails.cssOrigin = 'user';
        chrome.tabs.insertCSS(tabId, injectDetails,
                              chrome.test.callbackPass(function() {
          chrome.tabs.sendMessage(tabId, { id: 'user' },
                                  chrome.test.callbackPass(function(response) {
            chrome.test.assertEq('rgb(0, 0, 255)', response.color);
            // !important rules in user style sheets do override inline
            // !important rules.
            chrome.test.assertEq('rgb(255, 255, 255)',
                                 response.backgroundColor);
          }));
        }));
      },
      function noneOriginShouldSucceed() {
        // When no CSS origin is specified, it should default to author origin.
        var injectDetails = {};
        injectDetails.code = '#none {' +
                             ' color: blue !important;' +
                             ' background-color: white !important;' +
                             '}';
        chrome.tabs.insertCSS(tabId, injectDetails,
                              chrome.test.callbackPass(function() {
          chrome.tabs.sendMessage(tabId, { id: 'none' },
                                  chrome.test.callbackPass(function(response) {
            chrome.test.assertEq('rgb(0, 0, 255)', response.color);
            // !important rules in author style sheets do not override inline
            // !important rules.
            chrome.test.assertEq('rgb(0, 0, 0)', response.backgroundColor);
          }));
        }));
      },
      function unknownOriginShouldFail() {
        var injectDetails = {};
        injectDetails.code = '#unknown { color: black !important }';
        injectDetails.cssOrigin = 'unknown';
        try {
          chrome.tabs.insertCSS(tabId, injectDetails);
          chrome.test.fail('Unknown CSS origin should throw an error');
        } catch (e) {
          chrome.test.succeed();
        }
      },
      function originInExecuteScriptShouldFail() {
        var injectDetails = {};
        injectDetails.code = '(function(){})();';
        injectDetails.cssOrigin = 'author';
        chrome.tabs.executeScript(tabId, injectDetails,
            chrome.test.callbackFail(
                'CSS origin should be specified only for CSS code.'));
      }
    ]);
  });
  chrome.tabs.create({ url: testUrl });
});
