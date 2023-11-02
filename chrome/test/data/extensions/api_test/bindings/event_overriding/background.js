// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  var testWindowId = null;

  chrome.windows.create({url: 'page.html'}, function(w) {
    testWindowId = w.id;
    chrome.windows.remove(w.id);
  });

  // Regression test for http://crbug.com/436593 (dotted api names).
  chrome.system.storage.onAttached.addListener(function () {
  });

  chrome.windows.onRemoved.addListener(function listener(windowId) {
    if (windowId != testWindowId)
      return;  // I guess some other window might have closed?

    // If the event hasn't been overridden there should be a listener.
    chrome.test.assertTrue(chrome.windows.onRemoved.hasListeners());

    // This used to crash since we try to register the event more than once.
    chrome.windows.onRemoved.removeListener(listener);
    chrome.windows.onRemoved.addListener(listener);

    // Regression test for http://crbug.com/436593 (dotted api names).
    chrome.system.storage.onAttached.addListener(function () {
    })
    chrome.test.succeed();
  });
}

chrome.test.runTests([test]);
