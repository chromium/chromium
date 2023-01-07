// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Clipboard permission test for Chrome.
// browser_tests.exe --gtest_filter=ClipboardApiTest.Extension

// TODO(kalman): Consolidate this test script with the other clipboard tests.

function testDomCopy() {
  if (document.execCommand('copy'))
    chrome.test.succeed();
  else
    chrome.test.fail('execCommand("copy") failed');
}

function testDomPaste() {
  if (document.execCommand('paste'))
    chrome.test.succeed();
  else
    chrome.test.fail('execCommand("paste") failed');
}

function testCopyInIframe() {
  var ifr = document.createElement('iframe');
  document.body.appendChild(ifr);
  window.command = 'copy';
  ifr.contentDocument.write('<script src="iframe.js"></script>');
}

function testPasteInIframe() {
  var ifr = document.createElement('iframe');
  document.body.appendChild(ifr);
  window.command = 'paste';
  ifr.contentDocument.write('<script src="iframe.js"></script>');
}

function testDone(result) {
  if (result)
    chrome.test.succeed();
  else
    chrome.test.fail();
}

function testExecuteScriptCopyPaste(baseUrl) {
  var tabUrl = baseUrl + '/test_file.html';
  function runScript(tabId) {
    chrome.tabs.executeScript(tabId, {file: 'content_script.js'},
                              chrome.test.callbackPass(function() {
      chrome.tabs.sendMessage(tabId, "run",
                              chrome.test.callbackPass(function(result) {
        chrome.tabs.remove(tabId);
        chrome.test.assertEq('', result);
      }));
    }));
  }

  chrome.tabs.create({url: tabUrl}, chrome.test.callbackPass(function(newTab) {
    var done = chrome.test.listenForever(chrome.tabs.onUpdated,
                                         function(_, info, updatedTab) {
      if (updatedTab.id == newTab.id && info.status == 'complete') {
        runScript(newTab.id);
        done();
      }
    });
  }));
}

function testContentScriptCopyPaste(baseUrl) {
  var tabUrl = baseUrl + '/test_file_with_body.html';
  function runScript(tabId) {
    chrome.tabs.sendMessage(tabId, "run",
                            chrome.test.callbackPass(function(result) {
      chrome.tabs.remove(tabId);
      chrome.test.assertEq('', result);
    }));
  }

  chrome.tabs.create({url: tabUrl}, chrome.test.callbackPass(function(newTab) {
    var done = chrome.test.listenForever(chrome.tabs.onUpdated,
                                         function(_, info, updatedTab) {
      if (updatedTab.id == newTab.id && info.status == 'complete') {
        runScript(newTab.id);
        done();
      }
    });
  }));
}

function bindTest(test, param) {
  var result = test.bind(null, param);
  result.generatedName = test.name;
  return result;
}

chrome.test.getConfig(function(config) {
  var baseUrl = 'http://localhost:' + config.testServer.port + '/extensions';
  chrome.test.runTests([
    testDomCopy,
    testDomPaste,
    testCopyInIframe,
    testPasteInIframe,
    bindTest(testExecuteScriptCopyPaste, baseUrl),
    bindTest(testContentScriptCopyPaste, baseUrl)
  ]);
})
