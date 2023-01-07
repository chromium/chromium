// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var relativePath =
    '/extensions/api_test/executescript/basic/test_executescript.html';
var testUrl = 'http://a.com:PORT' + relativePath;
var testFailureUrl = 'http://b.com:PORT' + relativePath;
var firstEnter = true;

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);
  testFailureUrl = testFailureUrl.replace(/PORT/, config.testServer.port);

  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    if (!firstEnter) {
      return;
    }
    firstEnter = false;

    chrome.test.runTests([

      function executeJavaScriptCodeShouldSucceed() {
        var script_file = {};
        script_file.code = "document.title = 'executeScript';";
        chrome.tabs.executeScript(tabId, script_file, function() {
          chrome.tabs.get(tabId, pass(function(tab) {
            assertEq('executeScript', tab.title);
          }));
        });
      },

      function executeJavaScriptFileShouldSucceed() {
        var script_file = {};
        script_file.file = 'script1.js';
        chrome.tabs.executeScript(tabId, script_file, function() {
          chrome.tabs.get(tabId, pass(function(tab) {
            assertEq('executeScript1', tab.title);
          }));
        });
      },

      function insertCSSTextShouldSucceed() {
        var css_file = {};
        css_file.code = "p {display:none;}";
        chrome.tabs.insertCSS(tabId, css_file, function() {
          var script_file = {};
          script_file.file = 'script3.js';
          chrome.tabs.executeScript(tabId, script_file, function() {
            chrome.tabs.get(tabId, pass(function(tab) {
              assertEq('none', tab.title);
            }));
          });
        });
      },

      function insertCSSFileShouldSucceed() {
        var css_file = {};
        css_file.file = '1.css';
        chrome.tabs.insertCSS(tabId, css_file, function() {
          var script_file = {};
          script_file.file = 'script2.js';
          chrome.tabs.executeScript(tabId, script_file, function() {
            chrome.tabs.get(tabId, pass(function(tab) {
              assertEq('block', tab.title);
            }));
          });
        });
      },

      function insertCSSTextShouldNotAffectDOM() {
        chrome.tabs.insertCSS(tabId, {code: 'p {display: none}'}, function() {
          chrome.tabs.executeScript(
              tabId,
              {code: 'document.title = document.styleSheets.length'},
              function() {
                chrome.tabs.get(tabId, pass(function(tab) {
                  assertEq('0', tab.title);
                }));
             });
        });
      },

      function executeJavaScriptCodeShouldFail() {
        var doneListening =
            chrome.test.listenForever(chrome.tabs.onUpdated, onUpdated);
        chrome.tabs.update(tabId, {url: testFailureUrl});

        function onUpdated(updatedTabId, changeInfo, tab) {
          if (updatedTabId !== tabId || tab.status != 'complete' ||
             tab.url != testFailureUrl)
            return;
          var script_file = {};
          script_file.code = "document.title = 'executeScript';";
          // The error message should contain the URL of the site for which it
          // failed because the extension has the tabs permission.
          chrome.tabs.executeScript(tabId, script_file, fail(
              'Cannot access contents of url "' + testFailureUrl +
              '". Extension manifest must request permission to access this ' +
              'host.'));
          doneListening();
        }
      },

      function executeJavaScriptWithNoneValueShouldFail() {
        var script_file = {};
        chrome.tabs.executeScript(tabId, script_file, fail(
            'No source code or file specified.'));
      },

      function executeJavaScriptWithTwoValuesShouldFail() {
        var script_file = {};
        script_file.file = 'script1.js';
        script_file.code = 'var test = 1;';
        chrome.tabs.executeScript(tabId, script_file, fail(
            'Code and file should not be specified ' +
            'at the same time in the second argument.'));
      }
    ]);
  });

  chrome.tabs.create({ url: testUrl });
});
