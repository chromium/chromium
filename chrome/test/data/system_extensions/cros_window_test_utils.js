// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If we are running using SystemExtensionsApiBrowserTest then import the
// test interface.
importScripts(
  'keyboard_codes.mojom-lite.js', 'event_constants.mojom-lite.js',
  'geometry.mojom-lite.js',
  'cros_window_management_test_helper.test-mojom-lite.js')

globalThis.testHelper =
  new systemExtensionsTest.mojom.CrosWindowManagementTestHelperRemote;
testHelper.$.bindNewPipeAndPassReceiver().bindInBrowser('process');

// We assume a single window only and apply cros_window API methods at index 0.
async function assertSingleWindow() {
  let windows = await chromeos.windowManagement.getWindows();
  assert_equals(
      windows.length, 1,
      `util functions restricted to testing with a single window.`);
}

// Checks that setting fullscreen correctly sets the fullscreen attribute.
// We know the state of the window when fullscreen but not when reverting;
// thus setFullscreen(false) must manually test for the expected reverted state.
async function setFullscreenAndTest(fullscreen) {
  await assertSingleWindow();
  {
    let [window] = await chromeos.windowManagement.getWindows();
    await window.setFullscreen(fullscreen);
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

// Calls the moveTo function and checks that the origin is set correctly
// without affecting bounds.
async function moveToAndTest(x, y) {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  const originalWidth = window.width;
  const originalHeight = window.height;
  await window.moveTo(x, y);

  await assertWindowBounds(x, y, originalWidth, originalHeight);
}

// Calls the moveBy function and checks that the origin is shifted without
// affecting bounds.
async function moveByAndTest(deltaX, deltaY) {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  const originalX = window.screenLeft;
  const originalY = window.screenTop;
  const originalWidth = window.width;
  const originalHeight = window.height;
  await window.moveBy(deltaX, deltaY);

  await assertWindowBounds(
      originalX + deltaX, originalY + deltaY, originalWidth, originalHeight);
}

// Calls the resizeTo function and asserts that the bounds are set correctly
// without moving the origin.
async function resizeToAndTest(width, height) {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  const originalX = window.screenLeft;
  const originalY = window.screenTop;
  await window.resizeTo(width, height);

  await assertWindowBounds(originalX, originalY, width, height);
}

// Calls the resizeBy function and asserts that the bounds are shifted correctly
// without moving the origin.
async function resizeByAndTest(deltaWidth, deltaHeight) {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  const originalX = window.screenLeft;
  const originalY = window.screenTop;
  const originalWidth = window.width;
  const originalHeight = window.height;
  await window.resizeBy(deltaWidth, deltaHeight);

  await assertWindowBounds(
      originalX, originalY, originalWidth + deltaWidth,
      originalHeight + deltaHeight);
}

// Maximizes the window and checks the maximized state is set correctly.
async function maximizeAndTest() {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  await window.maximize();
  await assertWindowState("maximized");
}

// Minimizes the window and checks the minimized state is set correctly.
async function minimizeAndTest() {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  await window.minimize();
  await assertWindowState("minimized");
}

async function restoreAndTest() {
  await assertSingleWindow();
  let [window] = await chromeos.windowManagement.getWindows();
  await window.restore();
  await assertWindowState('normal');
}

async function focusAndTest() {
  await assertSingleWindow();

  {
    let [window] = await chromeos.windowManagement.getWindows();
    await window.focus();
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

async function assertWindowBounds(x, y, width, height) {
  await assertSingleWindow();

  let [window] = await chromeos.windowManagement.getWindows();
  assert_equals(window.screenLeft, x);
  assert_equals(window.screenTop, y);
  assert_equals(window.screenLeft, window.screenX);
  assert_equals(window.screenTop, window.screenY);
  assert_equals(window.width, width);
  assert_equals(window.height, height);
}
