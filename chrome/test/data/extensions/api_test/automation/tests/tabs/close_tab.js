// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [function testCloseTab() {
  getUrlFromConfig('index.html', function(url) {
    createTabAndWaitUntilLoaded(url, function(tab) {
      chrome.automation.getDesktop(function(desktop) {
        const url = tab.url || tab.pendingUrl;
        function doTestCloseTab() {
          const rootNode = desktop.find({attributes: {docUrl: url}});
          if (!rootNode || !rootNode.docLoaded) {
            return;
          }
          const button = rootNode.find({role: 'button'});
          assertEq(rootNode, button.root);

          // Poll until the root node doesn't have a role anymore
          // indicating that it really did get cleaned up.
          function checkSuccess() {
            if (rootNode.role === undefined && button.role === undefined &&
                button.root === null) {
              chrome.test.succeed();
            } else {
              setTimeout(checkSuccess, 10);
            }
          }
          chrome.tabs.remove(tab.id);
          checkSuccess();
        }

        const rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          doTestCloseTab();
          return;
        }
        desktop.addEventListener('loadComplete', doTestCloseTab);
      });
    });
  });
}];
chrome.test.runTests(allTests);
