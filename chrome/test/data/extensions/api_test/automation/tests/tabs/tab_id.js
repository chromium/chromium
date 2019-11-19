
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var originalActiveTab;

function createBackgroundTab(url, callback) {
  chrome.tabs.query({ active: true }, function(tabs) {
    chrome.test.assertEq(1, tabs.length);
    originalActiveTab = tabs[0];
    createTab(url, function(tab) {
      chrome.tabs.update(originalActiveTab.id, { active: true }, function() {
        callback(tab);
      });
    })
  });
}

function assertCorrectTab(rootNode) {
  var title = rootNode.docTitle;
  chrome.test.assertEq('Automation Tests', title);
  chrome.test.succeed();
}

function getTreeForBackgroundTab(foregroundTabRootNode, backgroundTab) {
  // We haven't cheated and loaded the test in the foreground tab.
  chrome.test.assertTrue(foregroundTabRootNode.docLoaded);
  var foregroundTabTitle = foregroundTabRootNode.docTitle;
  chrome.test.assertFalse(foregroundTabTitle == 'Automation Tests');

}

var allTests = [
  function testGetTabById() {
    getUrlFromConfig('index.html', function(url) {
      // Keep the NTP as the active tab so that we know we're requesting the
      // tab by ID rather than just getting the active tab still.
      createBackgroundTab(url, function(backgroundTab) {
        // Fetch the current foreground tab to compare with the background tab.
        chrome.automation.getTree(originalActiveTab.id,
                                  function(ntpRootNode) {
          chrome.test.assertEq(ntpRootNode, undefined,
                               "Can't get automation tree for NTP");

          chrome.automation.getTree(backgroundTab.id, function(rootNode) {
            chrome.test.assertFalse(rootNode === undefined,
                                    "Got automation tree for background tab");
            chrome.test.assertFalse(
                rootNode.docLoaded,
                "Load complete never fires unless tab is foregrounded");

            chrome.tabs.update(backgroundTab.id, { active: true }, function() {
              if (rootNode.docLoaded) {
                assertCorrectTab(rootNode);
                return;
              }

              rootNode.addEventListener('loadComplete', function() {
                assertCorrectTab(rootNode);
              });
            });
          });
        });
      });
    });
  }
];

chrome.test.runTests(allTests);
