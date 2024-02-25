// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testForceLayoutFiresFocus() {
    var node = rootNode.find({ role: 'button'});
    assertEq('button', node.role);
    rootNode.addEventListener('focus', (evt) => {
      if (evt.target.role !== 'button') {
        return;
      }

      // The underlying DOM button has not changed, but its layout has. This
      // listener ensures we at least get a focus event on a new valid
      // accessibility object.
      assertEq('button', evt.target.role);
      chrome.test.succeed();
    });
    node.focus();
  }
];

setUpAndRunTabsTests(allTests, 'force_layout.html');
