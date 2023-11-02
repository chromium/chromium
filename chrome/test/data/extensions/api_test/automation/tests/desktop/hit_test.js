// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testHitTestInDesktop() {
    var url = 'data:text/html,<!doctype html>' +
        encodeURI('<div>Don\'t Click Me</div>' +
                  '<button>Click Me</button>');
    var didHitTest = false;
    chrome.automation.getDesktop(function(desktop) {
      chrome.tabs.create({url: url});

      desktop.addEventListener('loadComplete', function(event) {
        if (didHitTest)
          return;
        if (event.target.url.indexOf('data:') >= 0) {
          var button = desktop.find({ attributes: { name: 'Click Me',
                                                    role: 'button' } });
          if (button) {
            didHitTest = true;
            button.addEventListener(EventType.CLICKED, function() {
              chrome.test.succeed();
            }, true);
            // Click just barely on the second button, very close
            // to the first button. This tests that we are converting
            // coordinate properly.
            var cx = button.location.left + 10;
            var cy = button.location.top + 10;
            desktop.hitTest(cx, cy, EventType.CLICKED);
          }
        }
      }, false);
    });
  },
];

chrome.test.runTests(allTests);
