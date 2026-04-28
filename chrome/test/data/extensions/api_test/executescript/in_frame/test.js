// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pass = chrome.test.callbackPass;
const fail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

const RELATIVE_PATH =
    '/extensions/api_test/executescript/in_frame/test_executescript.html';

let testUrl = '';

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (changeInfo.status != 'complete') {
    return;
  }

  chrome.test.runTests([
    function executeJavaScriptCodeInAllFramesShouldSucceed() {
      const scriptFile = {};
      // Note: using "var" in the script here because it can be injected
      // multiple times.
      scriptFile.code = 'var extensionPort = chrome.runtime.connect();';
      scriptFile.code = scriptFile.code +
          'extensionPort.postMessage({message: document.title});';
      scriptFile.allFrames = true;
      let counter = 0;
      let totalTitles = '';
      const done = pass();
      function verifyAndFinish() {
        assertEq(counter, 5);
        assertEq(totalTitles, 'frametest0test1test2test3');
        chrome.runtime.onConnect.removeListener(eventListener);
        done();
      }
      function eventListener(port) {
        port.onMessage.addListener(function(data) {
          counter++;
          assertTrue(counter <= 5);
          totalTitles += data.message;
          if (counter == 5) {
            verifyAndFinish();
          }
        });
      }
      chrome.runtime.onConnect.addListener(eventListener);
      chrome.tabs.executeScript(tabId, scriptFile);
    },

    function insertCSSTextInAllFramesShouldSucceed() {
      const cssFile = {};
      cssFile.code = 'p {display:none;}';
      cssFile.allFrames = true;
      let newStyle = '';
      let counter = 0;
      const done = pass();
      function verifyAndFinish() {
        assertEq(newStyle, 'nonenonenonenone');
        assertEq(counter, 4);
        chrome.runtime.onConnect.removeListener(eventListener);
        done();
      }
      function eventListener(port) {
        port.onMessage.addListener(function(data) {
          counter++;
          newStyle += data.message;
          if (counter == 4) {
            verifyAndFinish();
          }
        });
      }
      chrome.runtime.onConnect.addListener(eventListener);
      chrome.tabs.insertCSS(tabId, cssFile, function() {
        const scriptFile = {};
        scriptFile.file = 'script.js';
        scriptFile.allFrames = true;
        chrome.tabs.executeScript(tabId, scriptFile);
      });
    },
  ]);
});

chrome.test.getConfig(function(config) {
  testUrl = `http://a.com:${config.testServer.port}${RELATIVE_PATH}`;
  chrome.tabs.create({ url: testUrl });
});
