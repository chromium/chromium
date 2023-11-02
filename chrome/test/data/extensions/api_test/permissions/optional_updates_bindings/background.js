// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// window.html will turn around and call this when it loads.
// |otherWindow| is its window object.
function runTest(otherWindow) {
  chrome.test.runWithUserGesture(function() {
    chrome.permissions.request({permissions: ['alarms']}, function(granted) {
      chrome.test.assertTrue(granted);
      // Assert that the bindings have been updated on ourselves the background
      // page, and the tab that was created.
      var expectedAlarmsKeys = [
          'clear', 'clearAll', 'create', 'get', 'getAll', 'onAlarm'];
      [window, otherWindow].forEach(function(w) {
        chrome.test.assertEq(expectedAlarmsKeys,
                             Object.keys(w.chrome.alarms).sort());
      });
      chrome.test.succeed();
    });
  });
}

// This is the important part; creating a new window would cause the optional
// pemissions to be updated on itself, rather than both the window and the
// background page.
chrome.tabs.create({url: 'window.html'});
