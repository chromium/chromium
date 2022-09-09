// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testError() {
    chrome.automation.getTree(function(tree) {
      try {
        rootNode.focus();
      } catch (e) {
        chrome.test.assertEq('focus requires {"desktop": true} or' +
                ' {"interact": true} in the "automation" manifest key.',
            e.message);
        chrome.test.succeed();
      }
    });
  }
];

setUpAndRunTests(allTests)
