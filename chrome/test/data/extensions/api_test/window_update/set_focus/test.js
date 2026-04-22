// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function windowSetFocused() {
    const oldWin = await chrome.windows.getCurrent();
    chrome.test.assertTrue(oldWin.focused, 'Initial window should be focused');

    // Create a new window.
    const newWin = await chrome.windows.create({focused: false});
    chrome.test.assertFalse(
        newWin.focused, 'New window should not be focused initially');

    // Helper to wait for focus to transition to a specific window.
    // Includes a safety check to resolve immediately if focus is already there.
    const waitForFocus = async (targetId) => {
      // 1. Check if it's already focused (handles immediate OS focus).
      const win = await chrome.windows.get(targetId);
      if (win.focused) {
        return;
      }

      // 2. Otherwise, wait for the event.
      return new Promise((resolve) => {
        const listener = (windowId) => {
          if (windowId === targetId) {
            chrome.windows.onFocusChanged.removeListener(listener);
            resolve();
          }
        };
        chrome.windows.onFocusChanged.addListener(listener);

        // Double check again in case state changed during listener setup.
        chrome.windows.get(targetId).then(w => {
          if (w.focused) {
            chrome.windows.onFocusChanged.removeListener(listener);
            resolve();
          }
        });
      });
    };

    // Step 1: Focus the new window and verify.
    await chrome.windows.update(newWin.id, {focused: true});
    await waitForFocus(newWin.id);
    const win1 = await chrome.windows.get(newWin.id);
    chrome.test.assertTrue(
        win1.focused, 'New window should be focused after update');

    // Step 2: Focus the old window and verify (returning to the original
    // state).
    await chrome.windows.update(oldWin.id, {focused: true});
    await waitForFocus(oldWin.id);
    const win2 = await chrome.windows.get(oldWin.id);
    chrome.test.assertTrue(win2.focused, 'Old window should be refocused');

    // Cleanup.
    await chrome.windows.remove(newWin.id);
    chrome.test.succeed();
  },
]);
