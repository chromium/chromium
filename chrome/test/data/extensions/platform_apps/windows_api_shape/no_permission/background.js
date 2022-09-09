// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testWindowShape(testId, region) {
  var createOptions = { frame: 'none' };

  chrome.app.window.create('index.html',
                           createOptions,
                           chrome.test.callbackPass(function(win) {
      chrome.test.assertEq(typeof(win.setShape), 'undefined');
  }));
}

// All these tests are run without app.window.shape permission set.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.runTests([

    // Window tries to set shape without app.window.shape permission.
    function testSetShapeNoPerm() {
      testWindowShape('testSetShapeNoPerm', {rects: []});
    },

  ]);
});

