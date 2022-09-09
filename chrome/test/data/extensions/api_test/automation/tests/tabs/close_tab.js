// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testCloseTab() {
    getUrlFromConfig('index.html', function(url) {
      createTabAndWaitUntilLoaded(url, function(tab) {
        chrome.automation.getTree(function(rootNode) {
          function doTestCloseTab() {
            var button = rootNode.find({role: 'button'});
            assertEq(rootNode, button.root);

              // Poll until the root node doesn't have a role anymore
              // indicating that it really did get cleaned up.
              function checkSuccess() {
                if (rootNode.role === undefined && button.role === undefined &&
                    button.root === null) {
                  chrome.test.succeed();
                } else {
                  window.setTimeout(checkSuccess, 10);
                }
              }
              chrome.tabs.remove(tab.id);
              checkSuccess();
          }

          if (rootNode.docLoaded) {
            doTestCloseTab();
            return;
          }
          rootNode.addEventListener('loadComplete', doTestCloseTab);
        });
      });
    });
  }
]
chrome.test.runTests(allTests);
