// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Waits until the document_idle script has run in the child frame, because we
// test whether this happens. To prevent the test from stalling indefinitely, we
// start the test anyway even if the document_idle script was not detected in
// the child frame.

// Wait for at most 2 seconds.
var kMaxDelayMs = 2000;

var timeStart;
window.onload = function() {
  window.onload = null;
  timeStart = Date.now();
  tryStartTest();
};

function tryStartTest() {
  if (isChildFrameReady()) {
    // If document_idle scripts run, then this happens within a few 100ms.
    chrome.runtime.sendMessage('start the test');
  } else if (Date.now() - timeStart > kMaxDelayMs) {
    // Start the test even if the child frame's document_idle script was not
    // injected. This is expected (because we don't run document_end and
    // document_idle scripts in frames with a failed provisional load).
    console.error('Did not detect document_idle. Starting anyway!');
    chrome.runtime.sendMessage('start the test');
  } else {
    setTimeout(tryStartTest, 200);
  }
}

function isChildFrameReady() {
  try {
    // documentIdle is set by at_document_idle.js
    if (frames[0].documentIdle) {
      return true;
    }
  } catch (e) {}
  return false;
}
