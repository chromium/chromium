// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// |testTitle| needs to be the same as |kTestTitle| in declarative_apitest.cc.
var testTitle = ':TEST:';
var redirectDataURI = 'data:text/html;charset=utf-8,<html><head><title>' +
                      testTitle +
                      '<%2Ftitle><%2Fhtml>';

var rule = {
  conditions: [
    new chrome.declarativeWebRequest.RequestMatcher({
        url: {schemes: ['http']}})
  ],
  actions: [
    new chrome.declarativeWebRequest.RedirectRequest({
      redirectUrl: redirectDataURI
    })
  ]
};

function report(details) {
  if (chrome.extension.lastError) {
    chrome.test.log(chrome.extension.lastError.message);
  } else if (details.length < 1) {
    chrome.test.log('Error loading rules.');
  }
}

var activeTabId;

function navigateAndWait(url, callback) {
  var done =
      chrome.test.listenForever(chrome.tabs.onUpdated, function(_, info, tab) {
        if (tab.id == activeTabId && info.status == 'complete') {
          if (callback)
            callback(tab);
          done();
        }
      });
  chrome.tabs.update(activeTabId, {url: url});
}

function checkTitleCallback(tab) {
  chrome.test.assertEq(testTitle, tab.title);
}

chrome.test.runTests([
  function setUp() {
    chrome.windows.getAll(
      {populate: true},
      chrome.test.callbackPass(function(windows) {
        chrome.test.assertEq(1, windows.length);
        activeTabId = windows[0].tabs[0].id;
      }))
  },
  function checkTitle() {
    chrome.declarativeWebRequest.onRequest.addRules([rule],
      chrome.test.callbackPass(function(details) {
        report(details);
        navigateAndWait('http://www.example.com',
                        chrome.test.callbackPass(checkTitleCallback));
      }));
  }
]);
