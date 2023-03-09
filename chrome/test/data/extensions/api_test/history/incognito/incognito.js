// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryApiTest.Incognito

function addItem() {
  chrome.history.addUrl({url: 'http://www.a.com/'}, function() {
    chrome.test.sendScriptResult('success');
  });
}

function countItemsInHistory() {
  var query = {'text': ''};
  chrome.history.search(query, function(results) {
    chrome.test.sendScriptResult(results.length.toString());
  });
}

// Return a message to sync test with page load.
let message = chrome.extension.inIncognitoContext ?
  'incognito ready' : 'regular ready';
chrome.test.sendMessage(message);
