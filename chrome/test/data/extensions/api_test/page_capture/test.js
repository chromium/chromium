// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.extension.pageCapture.
// browser_tests.exe --gtest_filter=ExtensionPageCaptureApiTest.*

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

var testUrl = 'http://www.a.com:PORT' +
    '/extensions/api_test/page_capture/google.html';

function verifyPageCapture(data, isFile) {
  assertEq(undefined, chrome.runtime.lastError);
  assertTrue(data != null);
  // It should contain few KBs of data.
  assertTrue(data.size > 100);
  var reader = new FileReader();
  // Let's make sure it contains some well known strings.
  reader.onload = function(e) {
    var text = e.target.result;
    if (!isFile) {
      assertTrue(text.indexOf(testUrl) != -1);
      assertTrue(text.indexOf('logo.png') != -1);
    } else {
      assertTrue(text.indexOf('app_background_page') != -1);
      assertTrue(text.indexOf('service_worker') != -1);
    }
    // Run the GC so the blob is deleted.
    setTimeout(function() {
      gc();
    });
    setTimeout(function() {
      chrome.test.succeed();
    }, 0);
  };
  reader.readAsText(data);
}

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);

  chrome.test.runTests([
    function saveAsMHTML() {
      chrome.tabs.onUpdated.addListener(function listener(
          tabId, changeInfo, tab) {
        if (tab.status == 'complete') {
          chrome.tabs.onUpdated.removeListener(listener);
          chrome.pageCapture.saveAsMHTML({tabId: tab.id}, function(data) {
            verifyPageCapture(data, false);
          });
        }
      });
      chrome.tabs.create({url: testUrl});
    },

    function saveAsMHTML_FileAccessRequiredForFileUrls() {
      var captureUrl = config.testDataDirectory + '/';
      chrome.tabs.onUpdated.addListener(function listener(
          tabId, changeInfo, tab) {
        if (tab.status == 'complete' && tab.url == captureUrl) {
          chrome.tabs.onUpdated.removeListener(listener);
          chrome.pageCapture.saveAsMHTML({tabId: tab.id}, function(data) {
            if (config.customArg == 'ONLY_PAGE_CAPTURE_PERMISSION') {
              chrome.test.assertLastError(
                  `Don't have permissions required to capture this page.`);
              chrome.test.succeed();
            } else {
              verifyPageCapture(data, true /* isFile */);
            }
          });
        }
      });
      chrome.test.openFileUrl(captureUrl);
    },

    function saveAsMHTML_RestrictedUrlReturnsError() {
      chrome.tabs.onUpdated.addListener(function listener(
          tabId, changeInfo, tab) {
        if (tab.status == 'complete') {
          chrome.tabs.onUpdated.removeListener(listener);
          chrome.pageCapture.saveAsMHTML({tabId: tab.id}, function(data) {
            chrome.test.assertLastError(
                `Don't have permissions required to capture this page.`);
            chrome.test.succeed();
          });
        }
      });
      chrome.tabs.create({url: 'chrome://version'});
    },
  ]);
});
