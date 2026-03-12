// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var assertEq = chrome.test.assertEq;
var pass = chrome.test.callbackPass;

var NO_BOOKMARKS_PERMISSION =
    "You do not have permission to use 'bookmarks.getTree'.";

chrome.test.getConfig(function(config) {

  function doReq(domain, callback) {
    var url = domain + ":PORT/extensions/test_file.txt";
    url = url.replace(/PORT/, config.testServer.port);

    chrome.test.log("Requesting url: " + url);
    fetch(url).then(function(response) {
      assertEq(200, response.status);
      return response.text();
    }).then(function(text) {
      assertEq("Hello!", text);
      callback(true);
    }).catch(function(error) {
      chrome.test.log(error.toString());
      callback(false);
    });
  }

  chrome.test.runTests([
    function denyRequest() {
      chrome.permissions.request(
          {permissions: ['bookmarks'], origins: ['http://*.c.com/*']},
          pass(function(granted) {
            // They were not granted, and there should be no error.
            assertFalse(granted);
            assertTrue(chrome.runtime.lastError === undefined);

            // Make sure they weren't granted...
            chrome.permissions.contains(
                {permissions: ['bookmarks'], origins:['http://*.c.com/*']},
                pass(function(result) { assertFalse(result); }));

            assertEq(undefined, chrome.bookmarks);
            doReq('http://b.c.com/', pass(function(result) {
              assertFalse(result);
            }));
      }));
    },

    function noPromptForActivePermissions() {
      // We shouldn't prompt if the extension already has the permissions.
      chrome.permissions.request(
          {permissions: ["management"]},
          pass(function(granted) {
        assertTrue(granted);
      }));
    }
  ]);
});
