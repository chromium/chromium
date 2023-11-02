// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

var allTests = [
  function testLocationInWebView() {
    rootNode.addEventListener(EventType.LOAD_COMPLETE, function() {
      var outerButton = rootNode.find({ attributes: { name: 'Outer' } });
      var innerButton = rootNode.find({ attributes: { name: 'Inner' } });
      if (outerButton && innerButton) {
        var outerRect = outerButton.location;
        var innerRect = innerButton.location;

        // The outer button should be at (50, 150). Allow one pixel off
        // for rounding errors.
        assertTrue(Math.abs(outerRect.left - 50) <= 1);
        assertTrue(Math.abs(outerRect.top - 150) <= 1);

        // The inner button should be exactly 100 x 200 pixels offset from
        // the outer button.
        assertTrue(Math.abs(innerRect.left - outerRect.left - 100) <= 1);
        assertTrue(Math.abs(innerRect.top - outerRect.top - 200) <= 1);
        chrome.test.succeed();
      }
    }, false);

    chrome.app.window.create('webview_frame.html', {
      'innerBounds': {
        'left': 50,
        'top': 150,
        'width': 400,
        'height': 400
      }
    });
  }
];

chrome.automation.getDesktop(function(rootNodeArg) {
  window.rootNode = rootNodeArg;
  chrome.test.runTests(allTests);
});
