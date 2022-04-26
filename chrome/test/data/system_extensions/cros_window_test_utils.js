// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We assume a single window only and apply cros_window API methods at index 0.
async function assertSingleWindow() {
  let windows = await chromeos.windowManagement.windows();
  assert_equals(windows.length, 1,
      `util functions restricted to testing with a single window.`);
}

// Checks that setting fullscreen correctly sets the fullscreen attribute.
// We know the state of the window when fullscreen but not when reverting;
// thus setFullscreen(false) must manually test for the expected reverted state.
async function setFullscreenAndTest(fullscreen) {
  await assertSingleWindow();
  {
    let [window] = await chromeos.windowManagement.windows();
    window.setFullscreen(fullscreen);
  }

  if (fullscreen) {
    await assertWindowState("fullscreen");
    return;
  }

  {
    let [window] = await chromeos.windowManagement.windows();
    assert_false(window.isFullscreen, `setFullscreen() fail`);
  }
}

async function setBoundsAndTest(newBounds) {
  await assertSingleWindow();

  {
    let [window] = await chromeos.windowManagement.windows();
    window.setBounds(newBounds.x, newBounds.y,
        newBounds.width, newBounds.height);
  }

  {
    let [window] = await chromeos.windowManagement.windows();
    const actualBounds = window.bounds;
    assert_weak_equals(actualBounds, newBounds, `set bounds incorrectly`);
  }
}

// Maximizes the window and checks the maximized state is set correctly.
async function maximizeAndTest() {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.windows();
  window.maximize();
  await assertWindowState("maximized");
}

// Minimizes the window and checks the minimized state is set correctly.
async function minimizeAndTest() {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.windows();
  window.minimize();
  await assertWindowState("minimized");
}

async function focusAndTest() {
  await assertSingleWindow();

  {
    let [window] = await chromeos.windowManagement.windows();
    window.focus();
  }

  {
    let [window] = await chromeos.windowManagement.windows();
    assert_true(window.isFocused, `focus() failed to set focus`);
    assert_equals(
        window.visibilityState, 'shown', `focus() should make window visible`);
  }
}

// Asserts we have 1 window and that window is in correct state of:
// { "maximized", "minimized", "fullscreen", "normal" }
async function assertWindowState(state) {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.windows();
  assert_equals(window.isMaximized, state === "maximized",
      `window should be in the ${state} state`);
  assert_equals(window.isMinimized, state === "minimized",
      `window should be in the ${state} state`);
  assert_equals(window.isFullscreen, state === "fullscreen",
      `window should be in the ${state} state`);
  assert_equals(
      window.visibilityState, state === 'minimized' ? 'hidden' : 'shown',
      `window should be in the ${state} state`);
}
