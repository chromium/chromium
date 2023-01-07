// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function verifyException(expectedMessage, tabId) {
  function onDebuggerEvent(debuggee, method, params) {
    if (debuggee.tabId == tabId && method == 'Runtime.exceptionThrown') {
      var exception = params.exceptionDetails.exception;
      if (exception.value.indexOf(expectedMessage) > -1) {
        chrome.debugger.onEvent.removeListener(onDebuggerEvent);
        chrome.test.succeed();
      }
    }
  };
  chrome.debugger.onEvent.addListener(onDebuggerEvent);
  chrome.debugger.attach({ tabId: tabId }, "1.1", function() {
    // Enabling console provides both stored and new messages via the
    // Console.messageAdded event.
    chrome.debugger.sendCommand({ tabId: tabId }, "Runtime.enable");
  });
}

let openTab;

chrome.test.runTests([
  async function testExceptionInExtensionPage() {
    ({openTab} = await import('/_test_resources/test_util/tabs_util.js'));
    const tab = await openTab(chrome.runtime.getURL('extension_page.html'));
    verifyException('Exception thrown in extension page.', tab.id);
  },

  async function testExceptionInInjectedScript() {
    function injectScriptAndSendMessage(tab) {
      chrome.tabs.executeScript(
          tab.id,
          { file: 'content_script.js' },
          function() {
            verifyException('Exception thrown in injected script.', tab.id);
          });
    }

    chrome.test.getConfig(async function(config) {
      const testUrl =
          `http://localhost:${config.testServer.port}/` +
          'extensions/test_file.html';
      const tab = await openTab(testUrl);
      injectScriptAndSendMessage(tab);
    });
  }
]);
