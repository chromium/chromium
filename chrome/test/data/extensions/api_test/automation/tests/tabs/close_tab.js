// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests =
    [function testCloseTab() {
      getUrlFromConfig('index.html', function(url) {
        createTabAndWaitUntilLoaded(url, function(tab) {
          chrome.automation.getDesktop(function(desktop) {
            let url = tab.url || tab.pendingUrl;
            function doTestCloseTab() {
              let rootNode = desktop.find({attributes: {docUrl: url}});
              if (!rootNode || !rootNode.docLoaded) {
                return;
              }
              var button = rootNode.find({role: 'button'});
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

            let rootNode = desktop.find({attributes: {docUrl: url}});
            if (rootNode && rootNode.docLoaded) {
              doTestCloseTab();
              return;
            }
            desktop.addEventListener('loadComplete', doTestCloseTab);
          });
        });
      });
    }]
chrome.test.runTests(allTests);
