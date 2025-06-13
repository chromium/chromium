// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

// Tests for Controlled Frame navigation-related APIs.

promise_test(async (test) => {
  let controlledFrame = await createControlledFrame('/simple.html');
  assert_false(await controlledFrame.canGoBack());
  assert_false(await controlledFrame.canGoForward());

  const url = new URL(controlledFrame.src);
  url.pathname = '/title1.html';
  await navigateControlledFrame(controlledFrame, url.toString());
  assert_true(await controlledFrame.canGoBack());
  assert_false(await controlledFrame.canGoForward());

  const backPromise = createNavigationPromise(controlledFrame);
  assert_true(await controlledFrame.back());
  await backPromise;
  assert_false(await controlledFrame.canGoBack());
  assert_true(await controlledFrame.canGoForward());

  const forwardPromise = createNavigationPromise(controlledFrame);
  assert_true(await controlledFrame.forward());
  await forwardPromise;
  assert_true(await controlledFrame.canGoBack());
  assert_false(await controlledFrame.canGoForward());

  assert_false(await controlledFrame.forward());
  assert_false(await controlledFrame.go(5));
  assert_false(await controlledFrame.go(-5));
}, 'Navigation');
