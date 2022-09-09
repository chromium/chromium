// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var urlToLoad;

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(function(config) {
      urlToLoad = "http://a.com:" + config.testServer.port +
          "/extensions/api_test/service_worker/worker_based_background/" +
          "tabs_execute_script/empty.html";
      chrome.test.succeed();
    });
  },
  function testExecuteScriptInline() {
    chrome.tabs.create({url: urlToLoad}, function(tab) {
      chrome.tabs.executeScript({code: 'document.title;'}, function(res) {
        chrome.test.assertEq(undefined, chrome.runtime.lastError);
        chrome.test.assertEq('Execute Script Title', res[0]);
        chrome.test.succeed();
      });
    });
  },
  function testExecuteScriptFromFile() {
    chrome.tabs.create({url: urlToLoad}, function(tab) {
      chrome.tabs.executeScript({file: 'document_title.js'}, function(res) {
        chrome.test.assertEq(undefined, chrome.runtime.lastError);
        chrome.test.assertEq('Execute Script Title', res[0]);
        chrome.test.succeed();
      });
    });
  },
]);
