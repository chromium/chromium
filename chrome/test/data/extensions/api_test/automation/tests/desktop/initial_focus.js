// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testInitialFocus() {
    var url = 'data:text/html,<!doctype html>' +
        encodeURI('<input autofocus title=abc>');
    chrome.automation.getDesktop(function(rootNode) {
      rootNode.addEventListener('focus', function(event) {
        if (event.target.root.url == url) {
          chrome.automation.getFocus(function(focus) {
            if (focus.role !== 'textField') {
              // If the page is particularly slow in loading, the root may have
              // focus first. Wait for subsequent focus events.
              return;
            }

            assertEq('abc', focus.name);
            chrome.test.succeed();
          });
        }
      }, false);
    });

    chrome.tabs.create({url: url});
  },
];

chrome.test.runTests(allTests);
