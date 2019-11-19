// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertNoSensitiveFields(tab) {
  ['url', 'pendingUrl', 'title', 'favIconUrl'].forEach(function(field) {
    chrome.test.assertEq(undefined, tab[field],
                         'Sensitive property ' + field + ' is visible')
  });
}

var port;

function testUrl(domain) {
    return 'http://' + domain + ':' + port +
      '/extensions/test_file.html';
}

chrome.test.getConfig(function(config) {
  port = config.testServer.port;
  chrome.test.runTests([
    function testOnUpdated() {
      // two onUpdateListener calls, one create callback
      var neededCallbacks = 3;
      var countDown = function() {
        neededCallbacks--;
        if (neededCallbacks == 0) {
          chrome.tabs.onUpdated.removeListener(onUpdateListener);
          chrome.test.succeed();
        }
      };

      var onUpdateListener = function(tabId, info, tab) {
        assertNoSensitiveFields(info);
        assertNoSensitiveFields(tab);
        countDown();
      }

      chrome.tabs.onUpdated.addListener(onUpdateListener);
      chrome.tabs.create({url: 'chrome://newtab/'}, function(tab) {
        assertNoSensitiveFields(tab);
        chrome.tabs.update(tab.id, {url: 'about:blank'}, function(tab) {
          assertNoSensitiveFields(tab);
          countDown();
        });
      });
    },

    function testQuery() {
      chrome.tabs.create({url: 'chrome://newtab/'});
      chrome.tabs.query({active: true},
          chrome.test.callbackPass(function(tabs) {
        chrome.test.assertEq(1, tabs.length);
        assertNoSensitiveFields(tabs[0]);
      }));
    },

    function testErrorForCodeInjection() {
      chrome.tabs.create({url: testUrl('a.com')}, function(tab) {
        chrome.tabs.executeScript(tab.id, {code: ''},
          // Error message should *not* contain a page URL here because the
          // manifest file does not contain the tabs permission. Exposing
          // the URL without tabs permission would be a privacy leak.
          chrome.test.callbackFail('Cannot access contents of the page. ' +
              'Extension manifest must request permission to access the ' +
              'respective host.'));
      });
    },

  ]);
});
