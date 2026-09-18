// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function createPipWindow() {
  await new Promise((resolve, reject) => {
    chrome.test.runWithUserGesture(() => {
      window.documentPictureInPicture
          .requestWindow({
            width: 300,
            height: 250,
          })
          .then(resolve, reject);
    });
  });
  const pipWin = await chrome.windows.getAll().then(
      (windows) => windows.find((window) => window.alwaysOnTop));
  chrome.test.assertTrue(!!pipWin, 'PiP window should exist');
  return pipWin;
}

chrome.test.runTests([
  // Fullscreen, maximized and minimized are rejected for Picture-in-Picture
  // windows.
  async function testRejectOnDisallowedStates() {
    const win = await createPipWindow();
    for (const state of ['fullscreen', 'maximized', 'minimized']) {
      await chrome.test.assertPromiseRejects(
          chrome.windows.update(win.id, {state}),
          'Error: Operation not allowed for picture-in-picture windows');
    }
    await chrome.windows.remove(win.id);
    chrome.test.succeed();
  },

  // Resizing and repositioning is allowed.
  async function testUpdatePipBoundsAllowed() {
    const win = await createPipWindow();
    const updatedBounds = {
      left: win.left + 20,
      top: win.top + 20,
      width: win.width + 40,
      height: win.height + 40,
    };
    const updated = await chrome.windows.update(win.id, updatedBounds);
    chrome.test.assertEq(win.left + 20, updated.left);
    chrome.test.assertEq(win.top + 20, updated.top);
    chrome.test.assertEq(win.width + 40, updated.width);
    chrome.test.assertEq(win.height + 40, updated.height);
    await chrome.windows.remove(win.id);
    chrome.test.succeed();
  },

  // Bounds exceeding maximum allowed PiP size succeed and are clamped by the
  // window frame to its maximum size, set to 80% of display size by
  // PictureInPictureWindowManager::GetMaximumWindowSize() and enforced by
  // window->SetBounds().
  async function testUpdatePipAboveMaxSizeClamped() {
    const win = await createPipWindow();
    const tooLarge = {
      left: 0,
      top: 0,
      width: window.screen.width,
      height: window.screen.height,
    };
    const updated = await chrome.windows.update(win.id, tooLarge);
    chrome.test.assertTrue(updated.width < tooLarge.width);
    chrome.test.assertTrue(updated.height < tooLarge.height);
    await chrome.windows.remove(win.id);
    chrome.test.succeed();
  },

  // Bounds smaller than minimum allowed PiP size succeed and are clamped by the
  // window frame to its minimum size, set in
  // PictureInPictureWindow::GetMinimumInnerWindowSize() and enforced by
  // window->SetBounds().
  async function testUpdatePipBelowMinSizeClamped() {
    const win = await createPipWindow();
    const tooSmall = {left: 100, top: 100, width: 50, height: 50};
    const updated = await chrome.windows.update(win.id, tooSmall);
    chrome.test.assertTrue(updated.width > 50);
    chrome.test.assertTrue(updated.height > 50);
    await chrome.windows.remove(win.id);
    chrome.test.succeed();
  },
]);
