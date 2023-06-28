// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var callbackFail = chrome.test.callbackFail;

var urlA = '';
var urlB = '';

var allTestsADomain = [
  function testSuccess() {
    chrome.automation.getTree(function(tree) {
      assertFalse(tree === undefined);
      chrome.test.succeed();
    });
  }
];

var allTestsBDomain = [
  function testError() {
    var expectedError = 'Failed request of automation on a page';

    chrome.automation.getTree(callbackFail(expectedError, function(tree) {
      assertEq(undefined, tree);
      chrome.test.succeed();
    }));
  }
];

chrome.test.getConfig(function(config) {
  assertTrue('testServer' in config, 'Expected testServer in config');
  urlA = 'http://a.com:PORT/index.html'
      .replace(/PORT/, config.testServer.port);

  createTabAndWaitUntilLoaded(urlA, function(unused_tab) {
    chrome.test.runTests(allTestsADomain);

    urlB = 'http://b.com:PORT/index.html'
        .replace(/PORT/, config.testServer.port);

    createTabAndWaitUntilLoaded(urlB, function(unused_tab) {
      chrome.test.runTests(allTestsBDomain);
    });
  });
});
