// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var eventCounts = {};
var eventCallback = null;

function clearEventCounts() {
  eventCallback = null;
  eventCounts['onBoundsChanged'] = 0;
  eventCounts['onMinimized'] = 0;
  eventCounts['onMaximized'] = 0;
  eventCounts['onRestored'] = 0;
}

clearEventCounts();

chrome.app.window.create('main.html', function(win) {
  win.onBoundsChanged.addListener(function() {
    eventCounts['onBoundsChanged']++;
    if (eventCallback)
      eventCallback();
  });
  win.onMinimized.addListener(function() {
    eventCounts['onMinimized']++;
    if (eventCallback)
      eventCallback();
  });
  win.onMaximized.addListener(function() {
    eventCounts['onMaximized']++;
    if (eventCallback)
      eventCallback();
  });
  win.onRestored.addListener(function() {
    eventCounts['onRestored']++;
    if (eventCallback)
      eventCallback();
  });
});

