// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var called = false;
function testReceivedEvent() {
  chrome.test.assertTrue(called);

  chrome.runtime.getBackgroundPage(
      function(background_page) {
        chrome.test.assertTrue(background_page.called);
        chrome.test.succeed();
      });
}

chrome.bookmarks.onCreated.addListener(
    function() {
      called = true;
    });

chrome.test.sendMessage('ready',
    function(message) {
      chrome.test.runTests([testReceivedEvent]);
    });
