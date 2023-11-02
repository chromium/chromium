// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.display api test
// extensions_browsertests --gtest_filter=SystemDisplayApiTest.*

chrome.test.runTests([
  function testGet() {
    for(var i = 0; i < 10; ++i) {
      chrome.system.display.getInfo(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(4, result.length);
          for (var i = 0; i < result.length; i++) {
            var info = result[i];
            chrome.test.assertEq('' + i, info.id);
            chrome.test.assertEq('DISPLAY NAME FOR ' + i, info.name);
            chrome.test.assertEq(i == 0 ? true : false, info.isPrimary);
            chrome.test.assertEq(i == 0 ? true : false, info.isInternal);
            if (i == 1) {
              chrome.test.assertEq('0', info.mirroringSourceId);
            } else {
              chrome.test.assertEq('', info.mirroringSourceId);
            }
            chrome.test.assertEq(true, info.isEnabled);
            chrome.test.assertEq(90 * i, info.rotation);
            chrome.test.assertEq(96.0, info.dpiX);
            chrome.test.assertEq(96.0, info.dpiY);
            chrome.test.assertEq(0, info.bounds.left);
            chrome.test.assertEq(0, info.bounds.top);
            chrome.test.assertEq(1280, info.bounds.width);
            chrome.test.assertEq(720, info.bounds.height);
            if (i == 0) {
              chrome.test.assertEq(20, info.overscan.left);
              chrome.test.assertEq(40, info.overscan.top);
              chrome.test.assertEq(60, info.overscan.right);
              chrome.test.assertEq(80, info.overscan.bottom);
            } else {
              chrome.test.assertEq(0, info.overscan.left);
              chrome.test.assertEq(0, info.overscan.top);
              chrome.test.assertEq(0, info.overscan.right);
              chrome.test.assertEq(0, info.overscan.bottom);
            }
            chrome.test.assertEq(0, info.workArea.left);
            chrome.test.assertEq(0, info.workArea.top);
            chrome.test.assertEq(960, info.workArea.width);
            chrome.test.assertEq(720, info.workArea.height);
        }
      }));
    }
  }
]);

