// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
const pass = chrome.test.callbackPass;

chrome.test.getConfig(function(config) {
  const javaScriptURL = `javascript:void(document.title='js-url-success')`;

  const urlA =
      `http://a.com:${config.testServer.port}/extensions/test_file.html`;
  const urlB =
      `http://b.com:${config.testServer.port}/extensions/test_file.html`;

  chrome.tabs.create({url: urlA}, function(tab) {
    const firstTabId = tab.id;

    chrome.tabs.create({url: urlB}, function(tab) {
      const secondTabId = tab.id;

      chrome.test.runTests([
        function javaScriptURLShouldFail() {
          chrome.tabs.update(
              firstTabId, {url: javaScriptURL},
              chrome.test.callbackFail(
                  `Cannot access contents of url "${urlA}". Extension ` +
                  'manifest must request permission to access this host.'));
        },

        function javaScriptURLShouldSucceed() {
          chrome.tabs.update(
              secondTabId,
              {url: javaScriptURL},
              pass(function(tab) {
            assertEq(secondTabId, tab.id);
            assertEq('js-url-success', tab.title);
          }));
        }
      ]);
    });
  });
});
