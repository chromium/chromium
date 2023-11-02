// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var relativePath =
    '/extensions/api_test/executescript/in_frame/test_executescript.html';
var testUrl = 'http://a.com:PORT' + relativePath;

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (changeInfo.status != 'complete')
    return;

  chrome.test.runTests([
    function executeJavaScriptCodeInAllFramesShouldSucceed() {
      var script_file = {};
      script_file.code = "var extensionPort = chrome.runtime.connect();";
      script_file.code = script_file.code +
          "extensionPort.postMessage({message: document.title});";
      script_file.allFrames = true;
      var counter = 0;
      var totalTitles = '';
      var done = pass();
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
          if (counter == 5)
            verifyAndFinish();
        });
      };
      chrome.runtime.onConnect.addListener(eventListener);
      chrome.tabs.executeScript(tabId, script_file);
    },

    function insertCSSTextInAllFramesShouldSucceed() {
      var css_file = {};
      css_file.code = "p {display:none;}";
      css_file.allFrames = true;
      var newStyle = '';
      var counter = 0;
      var done = pass();
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
          if (counter == 4)
            verifyAndFinish();
        });
      };
      chrome.runtime.onConnect.addListener(eventListener);
      chrome.tabs.insertCSS(tabId, css_file, function() {
        var script_file = {};
        script_file.file = 'script.js';
        script_file.allFrames = true;
        chrome.tabs.executeScript(tabId, script_file);
      });
    }
  ]);
});

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);
  chrome.tabs.create({ url: testUrl });
});
