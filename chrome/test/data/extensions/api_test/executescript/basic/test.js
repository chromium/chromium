// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pass = chrome.test.callbackPass;
const fail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

const RELATIVE_PATH =
    '/extensions/api_test/executescript/basic/test_executescript.html';

let firstEnter = true;

chrome.test.getConfig(function(config) {
  const testUrl = `http://a.com:${config.testServer.port}${RELATIVE_PATH}`;
  const testFailureUrl =
      `http://b.com:${config.testServer.port}${RELATIVE_PATH}`;

  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete') {
      return;
    }
    if (!firstEnter) {
      return;
    }
    firstEnter = false;

    chrome.test.runTests([

      function executeJavaScriptCodeShouldSucceed() {
        const scriptFile = {};
        scriptFile.code = `document.title = 'executeScript';`;
        chrome.tabs.executeScript(tabId, scriptFile, function() {
          chrome.tabs.get(tabId, pass(function(tab) {
            assertEq('executeScript', tab.title);
          }));
        });
      },

      function executeJavaScriptFileShouldSucceed() {
        const scriptFile = {};
        scriptFile.file = 'script1.js';
        chrome.tabs.executeScript(tabId, scriptFile, function() {
          chrome.tabs.get(tabId, pass(function(tab) {
            assertEq('executeScript1', tab.title);
          }));
        });
      },

      function insertCSSTextShouldSucceed() {
        const cssFile = {};
        cssFile.code = 'p {display:none;}';
        chrome.tabs.insertCSS(tabId, cssFile, function() {
          const scriptFile = {};
          scriptFile.file = 'script3.js';
          chrome.tabs.executeScript(tabId, scriptFile, function() {
            chrome.tabs.get(tabId, pass(function(tab) {
              assertEq('none', tab.title);
            }));
          });
        });
      },

      function insertCSSFileShouldSucceed() {
        const cssFile = {};
        cssFile.file = '1.css';
        chrome.tabs.insertCSS(tabId, cssFile, function() {
          const scriptFile = {};
          scriptFile.file = 'script2.js';
          chrome.tabs.executeScript(tabId, scriptFile, function() {
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
        const doneListening =
            chrome.test.listenForever(chrome.tabs.onUpdated, onUpdated);
        chrome.tabs.update(tabId, {url: testFailureUrl});

        function onUpdated(updatedTabId, changeInfo, tab) {
          if (updatedTabId !== tabId || tab.status != 'complete' ||
              tab.url != testFailureUrl) {
            return;
          }
          const scriptFile = {};
          scriptFile.code = `document.title = 'executeScript';`;
          // The error message should contain the URL of the site for which it
          // failed because the extension has the tabs permission.
          chrome.tabs.executeScript(
              tabId, scriptFile,
              fail(
                  `Cannot access contents of url "${testFailureUrl}". ` +
                  'Extension manifest must request permission to access this ' +
                  'host.'));
          doneListening();
        }
      },

      function executeJavaScriptWithNoneValueShouldFail() {
        const scriptFile = {};
        chrome.tabs.executeScript(
            tabId, scriptFile, fail('No source code or file specified.'));
      },

      function executeJavaScriptWithTwoValuesShouldFail() {
        const scriptFile = {};
        scriptFile.file = 'script1.js';
        scriptFile.code = 'let test = 1;';
        chrome.tabs.executeScript(
            tabId, scriptFile,
            fail(
                'Code and file should not be specified ' +
                'at the same time in the second argument.'));
      },
    ]);
  });

  chrome.tabs.create({ url: testUrl });
});
