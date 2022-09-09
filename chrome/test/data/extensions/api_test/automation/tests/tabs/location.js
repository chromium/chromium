// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testLocation() {
    function assertOkButtonLocation(event) {
      var okButton = rootNode.find({ role: RoleType.BUTTON,
                                     attributes: { name: 'Ok' }});
      assertTrue('location' in okButton);

      // We can't assert the left and top positions because they're
      // returned in global screen coordinates. Just check the width and
      // height which may be clipped.
      assertTrue(okButton.location.width <= 30);
      assertTrue(okButton.location.height <= 30);
      chrome.test.succeed();
    };

    var okButton = rootNode.firstChild.firstChild;
    assertTrue('location' in okButton, 'no location in okButton');
    assertTrue('left' in okButton.location, 'no left in location');
    assertTrue('top' in okButton.location, 'no top in location');
    assertTrue('height' in okButton.location, 'no height in location');
    assertTrue('width' in okButton.location, 'no width in location');

    rootNode.addEventListener(
        EventType.LAYOUT_COMPLETE, assertOkButtonLocation);
    chrome.tabs.executeScript({ 'code':
          'document.querySelector("button")' +
          '.setAttribute("style", "position: absolute; left: 100; top: 150; ' +
          'width: 300; height: 350;");' });
  }
];

setUpAndRunTests(allTests);
