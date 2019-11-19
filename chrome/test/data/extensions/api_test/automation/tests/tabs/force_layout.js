// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testForceLayoutFiresFocus() {
    var node = rootNode.find({ role: 'button'});
    assertEq('button', node.role);
    rootNode.addEventListener('focus', (evt) => {
      // The underlying DOM button has not changed, but its layout has. This
      // listener ensures we at least get a focus event on a new valid
      // accessibility object. Once display none nodes are in the tree, it's
      // likely that |node| == |evt.target|.
      assertFalse(evt.target == node);
      assertEq(undefined, node.role);
      assertEq('button', evt.target.role);
      chrome.test.succeed();
    });
    node.focus();
  }
];

setUpAndRunTests(allTests, 'force_layout.html');
