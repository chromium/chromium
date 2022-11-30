// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;

// These tests are run with a device scale factor of 2.0, but all of the
// coordinates we get from the automation API should be with no device
// scale factor applied.
//
// Note that internally, the device scale factor is applied to web trees,
// but not to the desktop tree, so we want to make sure that that's properly
// taken into account when converting from relative coordinates to global
// device-independent coordinates.
var allTests = [
  function testLocationInWebView() {
    rootNode.addEventListener(EventType.LOAD_COMPLETE, function() {
      var firstButton = rootNode.find({ attributes: { name: 'First' } });
      var secondButton = rootNode.find({ attributes: { name: 'Second' } });
      if (firstButton && secondButton) {
        var firstRect = firstButton.location;
        var secondRect = secondButton.location;

        // The first button should be at (50, 150) with a size of (200, 100).
        // Allow one pixel off for rounding errors.
        assertTrue(Math.abs(firstRect.left - 50) <= 1);
        assertTrue(Math.abs(firstRect.top - 150) <= 1);
        assertTrue(Math.abs(firstRect.width - 200) <= 1);
        assertTrue(Math.abs(firstRect.height - 100) <= 1);

        // The second button should be exactly 225 x 25 pixels offset from
        // the first button.
        assertTrue(Math.abs(secondRect.left - firstRect.left - 225) <= 1);
        assertTrue(Math.abs(secondRect.top - firstRect.top - 25) <= 1);
        assertTrue(Math.abs(secondRect.width - 200) <= 1);
        assertTrue(Math.abs(secondRect.height - 100) <= 1);
        chrome.test.succeed();
      }
    }, false);

    chrome.app.window.create('buttons.html', {
      'innerBounds': {
        'left': 50,
        'top': 150,
        'width': 800,
        'height': 800
      }
    });
  }
];

chrome.automation.getDesktop(function(rootNodeArg) {
  window.rootNode = rootNodeArg;
  chrome.test.runTests(allTests);
});
