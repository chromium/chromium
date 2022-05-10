// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We assume a single window only and apply cros_window API methods at index 0.
async function assertSingleWindow() {
  let windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 1,
      `util functions restricted to testing with a single window.`);
}

// Checks that setting fullscreen correctly sets the fullscreen attribute.
// We know the state of the window when fullscreen but not when reverting;
// thus setFullscreen(false) must manually test for the expected reverted state.
async function setFullscreenAndTest(fullscreen) {
  await assertSingleWindow();
  {
    let [window] = await chromeos.windowManagement.getWindows();
    window.setFullscreen(fullscreen);
  }

  if (fullscreen) {
    await assertWindowState("fullscreen");
    return;
  }

  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_not_equals(
        window.windowState, 'fullscreen', `unset fullscreen fail`);
  }
}

async function setOriginAndTest(x, y) {
  await assertSingleWindow();

  let originalWidth, originalHeight;

  {
    let [window] = await chromeos.windowManagement.getWindows();
    originalWidth = window.width;
    originalHeight = window.height;
    window.setOrigin(x, y);
  }

  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_equals(
        window.screenLeft, x,
        `SetOrigin should set origin without changing bounds`);
    assert_equals(
        window.screenTop, y,
        `SetOrigin should set origin without changing bounds`);
    assert_equals(
        window.width, originalWidth,
        `SetOrigin should set origin without changing bounds`);
    assert_equals(
        window.height, originalHeight,
        `SetOrigin should set origin without changing bounds`);
  }
}

async function setBoundsAndTest(x, y, width, height) {
  await assertSingleWindow();

  {
    let [window] = await chromeos.windowManagement.getWindows();
    window.setBounds(x, y, width, height);
  }

  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_equals(window.screenLeft, x, `set bounds incorrectly`);
    assert_equals(window.screenTop, y, `set bounds incorrectly`);
    assert_equals(window.width, width, `set bounds incorrectly`);
    assert_equals(window.height, height, `set bounds incorrectly`);
  }
}

// Maximizes the window and checks the maximized state is set correctly.
async function maximizeAndTest() {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  window.maximize();
  await assertWindowState("maximized");
}

// Minimizes the window and checks the minimized state is set correctly.
async function minimizeAndTest() {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  window.minimize();
  await assertWindowState("minimized");
}

async function focusAndTest() {
  await assertSingleWindow();

  {
    let [window] = await chromeos.windowManagement.getWindows();
    window.focus();
  }

  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_true(window.isFocused, `focus() failed to set focus`);
    assert_equals(
        window.visibilityState, 'shown', `focus() should make window visible`);
  }
}

// Asserts we have 1 window and that window is in correct state of:
// { "maximized", "minimized", "fullscreen", "normal" }
async function assertWindowState(state) {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  assert_equals(
      window.windowState, state, `window should be in ${state} state`);
  assert_equals(
      window.visibilityState, state === 'minimized' ? 'hidden' : 'shown',
      `window should be in the ${state} state`);
}
