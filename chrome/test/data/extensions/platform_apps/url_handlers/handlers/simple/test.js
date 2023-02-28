// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function (launchData) {
  // Test that the isKioskSession field is |false| and the id and items fields
  // can be read in the launch data.
  chrome.test.runTests([
    function testUrlHandler() {
      chrome.test.assertNe('undefined', typeof launchData, "No launchData");
      chrome.test.assertFalse(launchData.isKioskSession,
          "launchData.isKioskSession incorrect");
      chrome.test.assertEq(launchData.id, "my_doc_url",
          "launchData.id incorrect");
      chrome.test.assertTrue(typeof launchData.items == 'undefined',
          "Launched for file_handlers, not url_handlers");
      chrome.test.assertTrue(typeof launchData.url != 'undefined',
          "No url in launchData");
      chrome.test.assertTrue(typeof launchData.referrerUrl != 'undefined',
          "No referrerUrl in launchData");
      chrome.test.assertFalse(
          !launchData.url.match(
               /http:\/\/.*:.*\/extensions\/platform_apps\/url_handlers\/.*/),
          "Wrong launchData.url");
    }
  ]);

  chrome.app.window.create(
    "main.html",
    { },
    function(win) {
      win.contentWindow.onload = function() {
        var link = this.document.querySelector('#link');
        link.href = launchData.url;
        link.innerText = launchData.url;
        chrome.test.sendMessage("Handler launched");
      }
    });
});
