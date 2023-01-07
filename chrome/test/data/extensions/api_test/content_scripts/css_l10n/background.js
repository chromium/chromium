// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.test.log('Creating tab...');

  var URL = 'http://localhost:PORT/extensions/test_file_with_body.html';
  var TEST_FILE_URL = URL.replace(/PORT/, config.testServer.port);

  chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    chrome.tabs.onUpdated.removeListener(listener);

    // We need to test two different paths, because the message bundles used
    // for localization are loaded differently in each case:
    //   (1) localization upon loading extension scripts
    //   (2) localization upon injecting CSS with JavaScript
    chrome.test.runTests([
      // Tests that CSS loaded automatically from the files specified in the
      // manifest has had __MSG_@@extension_id__ replaced with the actual
      // extension id.
      function extensionIDMessageGetsReplacedInContentScriptCSS() {
        chrome.test.listenOnce(chrome.runtime.onMessage, function(message) {
          chrome.test.assertEq('extension_id', message.tag);
          chrome.test.assertEq('passed', message.message);
        });
        chrome.tabs.executeScript(tabId, {file: 'test_extension_id.js'});
      },

      // First injects CSS into the page through chrome.tabs.insertCSS and then
      // checks that it has had __MSG_text_color__ replaced with the correct
      // message value.
      function textDirectionMessageGetsReplacedInInsertCSSCall() {
        chrome.test.listenOnce(chrome.runtime.onMessage, function(message) {
          chrome.test.assertEq('paragraph_style', message.tag);
          chrome.test.assertEq('passed', message.message);
        });
        chrome.tabs.insertCSS(tabId, {file: 'test.css'}, function() {
          chrome.tabs.executeScript(tabId, {file: 'test_paragraph_style.js'});
        });
      }
    ]);
  });

  chrome.tabs.create({ url: TEST_FILE_URL });
});
