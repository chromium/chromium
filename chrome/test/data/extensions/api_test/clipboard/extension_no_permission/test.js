// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Clipboard permission test for Chrome.
// browser_tests.exe --gtest_filter=ClipboardApiTest.ExtensionNoPermission

// TODO(kalman): Consolidate this test script with the other clipboard tests.

var pass = chrome.test.callbackPass;

function testDomCopy() {
  if (document.execCommand('copy'))
    chrome.test.succeed();
  else
    chrome.test.fail('execCommand("copy") failed');
}

function testDomPaste() {
  if (document.execCommand('paste'))
    chrome.test.fail('execCommand("paste") succeeded');
  else
    chrome.test.succeed();
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
  // 'copy' should always succeed regardless of the clipboardWrite permission,
  // for backwards compatibility. 'paste' should always fail because the
  // extension doesn't have clipboardRead.
  var expected = window.command === 'copy';
  if (result === expected)
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

  chrome.tabs.create({url: tabUrl}, pass(function(newTab) {
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
});
