// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var pass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var listenForever = chrome.test.listenForever;

var testTabId;
var port;

function testUrl(domain) {
  return 'http://' + domain + ':' + port +
      '/extensions/test_file.html';
}

function error(domain) {
  return 'Cannot access contents of url "' + testUrl(domain) + '".' +
    ' Extension manifest must request permission to access this host.';
}

// Creates a new tab, navigated to the specified |domain|.
function createTestTab(domain, callback) {
  var createdTabId = -1;
  var done = listenForever(
      chrome.tabs.onUpdated,
      function(tabId, changeInfo, tab) {
    if (tabId == createdTabId && changeInfo.status != 'loading') {
      callback(tab);
      done();
    }
  });

  chrome.tabs.create({url: testUrl(domain)}, pass(function(tab) {
    createdTabId = tab.id;
  }));
}

chrome.test.getConfig(function(config) {
  port = config.testServer.port;
  chrome.test.runTests([

    // Before enabling the optional host permission, we shouldn't be able to
    // inject content scripts.
    function noAccess() {
      createTestTab('a.com', pass(function(tab) {
        testTabId = tab.id;
        chrome.tabs.executeScript(
            tab.id, {code: 'document.title = "success"'},
            callbackFail(error('a.com')));
      }));
    },

    // Add the host permission and see if we can inject a content script into
    // existing and new tabs.
    function addPermission() {
      chrome.permissions.request(
          {origins: ["http://*/*"]},
          pass(function(granted) {
        assertTrue(granted);

        // Try accessing the existing tab.
        chrome.tabs.executeScript(
            testTabId, {code: 'document.title = "success"'},
            pass(function() {
          chrome.tabs.get(testTabId, pass(function(tab) {
            assertEq('success', tab.title);
          }));
        }));

        // Make sure we can inject a script into a new tab with that host.
        createTestTab('a.com', pass(function(tab) {
          chrome.tabs.executeScript(
              tab.id, {code: 'document.title = "success"'},
              pass(function() {
            chrome.tabs.get(tab.id, pass(function(tab) {
              assertEq('success', tab.title);
            }));
          }));
        }));
      }));
    },

    // Try the host again, except outside of the permissions.request callback.
    function sameHost() {
      createTestTab('a.com', pass(function(tab) {
        chrome.tabs.executeScript(
            tab.id, {code: 'document.title = "success"'},
            pass(function() {
          chrome.tabs.get(tab.id, pass(function(tab) {
            assertEq('success', tab.title);
          }));
        }));
      }));
    },

    // Try injecting the script into a new tab with a new host.
    function newHost() {
      createTestTab('b.com', pass(function(tab) {
        chrome.tabs.executeScript(
            tab.id, {code: 'document.title = "success"'},
            pass(function() {
          chrome.tabs.get(tab.id, pass(function(tab) {
            assertEq('success', tab.title);
          }));
        }));
      }));
    }
  ]);
});
