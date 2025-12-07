// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var count = 0;
var bounds = {top: 0, left: 0, width: 0, height: 0};

chrome.windows.onBoundsChanged.addListener(function(window) {
  ++count;
  bounds.top = window.top;
  bounds.left = window.left;
  bounds.width = window.width;
  bounds.height = window.height;
});

chrome.test.sendMessage('ready', function (actualBounds) {
  var actualBounds = JSON.parse(actualBounds);

  chrome.test.assertEq(1, count);
  chrome.test.assertEq(bounds.top, actualBounds.top);
  chrome.test.assertEq(bounds.left, actualBounds.left);
  chrome.test.assertEq(bounds.width, actualBounds.width);
  chrome.test.assertEq(bounds.height, actualBounds.height);

  chrome.test.notifyPass();
});
