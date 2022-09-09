// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  // Basic query from root node.
  function testQuerySelector() {
    var cancelButton = rootNode.children[4];
    assertEq(chrome.automation.RoleType.BUTTON, cancelButton.role);
    function assertCorrectResult(queryResult) {
      assertEq(queryResult, cancelButton);
      chrome.test.succeed();
    }
    rootNode.domQuerySelector('body > button:nth-of-type(2)',
                              assertCorrectResult);
  },

  function testQuerySelectorNoMatch() {
    function assertCorrectResult(queryResult) {
      assertEq(null, queryResult);
      chrome.test.succeed();
    }
    rootNode.domQuerySelector('#nonexistent',
                              assertCorrectResult);
  },

  // Demonstrates that a query from a non-root element queries inside that
  // element.
  function testQuerySelectorFromMain() {
    var main = rootNode.children[1];
    assertEq(chrome.automation.RoleType.MAIN, main.role);
    // paragraph inside "main" element - not the first <p> on the page
    var p = main.firstChild;
    assertTrue(Boolean(p));
    function assertCorrectResult(queryResult) {
      assertEq(queryResult, p);
      chrome.test.succeed();
    }
    main.domQuerySelector('p', assertCorrectResult);
  },

  // Demonstrates that a query for an element, where the first match is ignored
  // for accessibility returns the a later unignored match.
  function testQuerySelectorFindsFirstUnignoredMatch() {
    var unignored = rootNode.children[5]; // <div class="findme">
    assertEq(chrome.automation.RoleType.GROUP, unignored.role);
    function assertCorrectResult(queryResult) {
      assertEq(unignored, queryResult);
      assertEq('findme', unignored.className);
      chrome.test.succeed();
    }
    rootNode.domQuerySelector('.findme', assertCorrectResult);
  },

  function testQuerySelectorForIgnoredReturnsNull() {
    function assertCorrectResult(queryResult) {
      assertEq(null, queryResult);
      chrome.test.succeed();
    }
    rootNode.domQuerySelector('#span-in-button', assertCorrectResult);
  },

  function testQuerySelectorFromRemovedNode() {
    var group = rootNode.firstChild;
    assertEq(chrome.automation.RoleType.GROUP, group.role);
    function assertCorrectResult(queryResult) {
      assertEq(null, queryResult);
      var errorMsg =
          'domQuerySelector sent on node which is no longer in the tree.';
      assertEq(errorMsg, chrome.extension.lastError.message);
      assertEq(errorMsg, chrome.runtime.lastError.message);

      chrome.test.succeed();
    }
    function afterRemoveChild() {
      group.domQuerySelector('h1', assertCorrectResult);
    }
    chrome.tabs.executeScript(
        { code: 'document.body.removeChild(document.body.firstElementChild)' },
        afterRemoveChild);
  }
];

setUpAndRunTests(allTests, 'complex.html');
