// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstButton = undefined;
var clicked = false;

function checkNodes(rootNode) {
  // Grab the first button and hold on to it.
  if (!firstButton) {
     firstButton = findAutomationNode(rootNode, function(n) {
       return n.name == 'First Button'
     });
  }

  // Search for second button.
  const secondButton = findAutomationNode(rootNode, function(n) {
     return n.name == 'Second Button'
  });

  // If we have the first but not the second, click the
  // switch button to swap out the frame.
  if (firstButton && !secondButton && !clicked) {
    firstButton.doDefault();
    clicked = true;
  }

  // Still waiting for expected state. If neither button
  // was found yet, keep polling.
  if (!firstButton || firstButton.role || !secondButton) {
     setTimeout(checkNodes.bind(this, rootNode), 100);
     return;
  }

  // Repetitive check with the above condition, but to make it clear to the
  // reader what's being tested. If the first button's role is still valid,
  // the test failed because its tree should have been destroyed.
  chrome.test.assertTrue(!!firstButton && !!secondButton);
  chrome.test.assertTrue(!firstButton.role && !!secondButton.role);
  chrome.test.succeed();
}

var allTests = [
  function treeDestroyedTest() {
    chrome.test.getConfig(function(config) {
      const url = 'http://a.com:' + config.testServer.port + '/iframenav/iframe-top.html';
      chrome.tabs.create({url: url});

      chrome.automation.getDesktop(checkNodes.bind(this));
    });
  },
];

chrome.test.runTests(allTests);
