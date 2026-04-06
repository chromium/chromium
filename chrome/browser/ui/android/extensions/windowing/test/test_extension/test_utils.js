// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// --- Custom Test Functions ---

// Test function to verify window focus behavior.
// Call this directly from the Service Worker DevTools console:
// testFocusChange(123);
async function testFocusChange(windowId) {
  try {
    console.log(`Focusing window ${windowId}...`);
    const focusResult =
        await chrome.windows.update(windowId, { focused: true });
    console.log("Focus update return:", focusResult);

    // Wait 2 seconds
    await new Promise(resolve => setTimeout(resolve, 2000));

    console.log(`Unfocusing window ${windowId}...`);
    const unfocusResult =
        await chrome.windows.update(windowId, { focused: false });
    console.log("Unfocus update return:", unfocusResult);

  } catch (error) {
    console.error("Error updating window:", error);
  }
}