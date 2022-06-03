// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [function testAccessibilityFocus() {
  const textNode = rootNode.find({attributes: {name: 'Text'}});
  textNode.setAccessibilityFocus();

  chrome.automation.getAccessibilityFocus((focusedNode) => {
    assertEq(textNode, focusedNode);

    textNode.addEventListener('locationChanged', (evt) => {
      assertEq(textNode, evt.target);
      chrome.test.succeed();
    });

    rootNode.find({role: 'button'}).doDefault();
  });
}];

setUpAndRunTests(allTests, 'accessibility_focus.html');
