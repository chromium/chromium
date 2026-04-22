// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// extension api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.ChromeRuntimeUninstallURL

const pass = chrome.test.callbackPass;
const UNINSTALL_URL = 'https://www.google.com/';
const SETS_UNINSTALL_URL = 'Sets Uninstall Url';
let uninstalled = false;
chrome.test.runTests([
  function uninstallURL() {
    chrome.management.getAll(function(results) {
      for (let i = 0; i < results.length; i++) {
        if (results[i].name == SETS_UNINSTALL_URL) {
          chrome.test.runWithUserGesture(pass(function() {
            chrome.management.uninstall(
                results[i].id, pass(function() {
                  chrome.tabs.query({url: UNINSTALL_URL}, pass(function(tabs) {
                                      chrome.test.assertEq(1, tabs.length);
                                      chrome.test.assertEq(
                                          UNINSTALL_URL,
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
      chrome.test.assertLastError(`Invalid URL: "chrome://newtab".`);
      chrome.test.succeed();
    });
  },
]);
