// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests that attaching a named event twice will fail.
  function doubleAttach() {
    function dummy() {};
    var onClicked = new chrome.Event("browserAction.onClicked");
    var onClicked2 = new chrome.Event("browserAction.onClicked");
    onClicked.addListener(dummy);
    chrome.test.assertTrue(onClicked.hasListeners());
    try {
      onClicked2.addListener(dummy);
      chrome.test.fail();
    } catch (e) {
      chrome.test.assertTrue(
          e.message.search("already attached") >= 0,
          e.message);
    }
    chrome.test.assertFalse(onClicked2.hasListeners());
    onClicked2.removeListener(dummy);

    onClicked.removeListener(dummy);
    chrome.test.assertFalse(onClicked.hasListeners());
    chrome.test.succeed();
  },

  // Tests that 2 pages attaching to the same event does not trigger a DCHECK.
  function twoPageAttach() {
    // Test harness should already have opened tab.html, which registers this
    // listener.
    chrome.browserAction.onClicked.addListener(function() {});

    // Test continues in twoPageAttach.html.
    window.open("twoPageAttach.html");
  },
]);
