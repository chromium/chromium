// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// extension api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ChromeRuntimeUninstallURL

var pass = chrome.test.callbackPass;
var uninstall_url = 'http://www.google.com/';
var sets_uninstall_url = 'Sets Uninstall Url';
var uninstalled = false;
chrome.test.runTests([
  function uninstallURL() {
    chrome.management.getAll(function(results) {
      for(var i = 0; i < results.length; i++) {
        if (results[i].name == sets_uninstall_url) {
          chrome.test.runWithUserGesture(pass(function() {
            chrome.management.uninstall(results[i].id, pass(function() {
              chrome.tabs.query({'url': uninstall_url}, pass(function(tabs) {
                chrome.test.assertEq(1, tabs.length);
                chrome.test.assertEq(uninstall_url,
                                     tabs[0].pendingUrl || tabs[0].url);
              }));
            }));
          }));
          uninstalled = true;
          break;
        }
      }
      chrome.test.assertTrue(uninstalled);
    });
  },
  function setEmptyUrl() {
    chrome.runtime.setUninstallURL('', function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  function uninstallInvalidURLNonHttpOrHttps() {
    chrome.runtime.setUninstallURL('chrome://newtab', function() {
      chrome.test.assertLastError('Invalid URL: "chrome://newtab".');
      chrome.test.succeed();
    });
  }
]);
