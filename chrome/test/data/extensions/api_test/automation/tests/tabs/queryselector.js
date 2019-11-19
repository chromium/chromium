// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  // Basic query from root node.
  function testQuerySelector() {
    var cancelButton = rootNode.children[4];
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
    // paragraph inside "main" element - not the first <p> on the page
    var p = main.firstChild;
    function assertCorrectResult(queryResult) {
      assertEq(queryResult, p);
      chrome.test.succeed();
    }
    main.domQuerySelector('p', assertCorrectResult);
  },

  // Demonstrates that a query for an element which is ignored for accessibility
  // returns its nearest ancestor.
  function testQuerySelectorForSpanInsideImageReturnsImage() {
    var img = rootNode.children[2];
    function assertCorrectResult(queryResult) {
      assertEq(queryResult, img);
      chrome.test.succeed();
    }
    rootNode.domQuerySelector('#span-in-img', assertCorrectResult);
  },

  function testQuerySelectorForSpanInsideButton() {
    var span = rootNode.children[3].children[0];
    function assertCorrectResult(queryResult) {
      assertEq(queryResult, span);
      chrome.test.succeed();
    }
    rootNode.domQuerySelector('#span-in-button', assertCorrectResult);
  },

  function testQuerySelectorFromRemovedNode() {
    var group = rootNode.firstChild;
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
