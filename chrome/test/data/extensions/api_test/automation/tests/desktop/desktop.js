// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testGetDesktop() {
    chrome.automation.getDesktop(function(rootNode) {
      assertEq(RoleType.DESKTOP, rootNode.role);
      chrome.test.succeed();
    });
  },

  function testGetDesktopTwice() {
    var desktop = null;
    chrome.automation.getDesktop(function(rootNode) {
      desktop = rootNode;
    });
    chrome.automation.getDesktop(function(rootNode) {
      assertEq(rootNode, desktop);
      chrome.test.succeed();
    });
  },

  function testGetDesktopNested() {
    var desktop = null;
    chrome.automation.getDesktop(function(rootNode) {
      desktop = rootNode;
      chrome.automation.getDesktop(function(rootNode2) {
        assertEq(rootNode2, desktop);
        chrome.test.succeed();
      });
    });
  }
];

chrome.test.runTests(allTests);
