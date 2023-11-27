// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [function testAutomationNodeProperties() {
  const automationRootNode = rootNode;
  const automationNode = automationRootNode.firstChild;

  // Property accesses on this node will trigger native C++ property getters
  // which should not crash.
  for (let name in automationRootNode) {
    automationRootNode[name];
  }

  for (let name in automationNode) {
    automationNode[name];
  }

  chrome.test.succeed();
}];

setUpAndRunTabsTests(allTests);
