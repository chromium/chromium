// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

const getControlledFrameUserAgent =
    async function(controlledFrame) {
  const checkScript = `
  (() => {
    return navigator.userAgent;
  })()`;
  return await executeAsyncScript(controlledFrame, checkScript);
}

const setUserAgentOverrideAndAwaitReload =
    async function(controlledframe, userAgent) {
  await new Promise((resolve, reject) => {
    if (!('src' in controlledframe)) {
      reject('FAIL');
      return;
    }
    controlledframe.addEventListener('loadstop', resolve);
    controlledframe.addEventListener('loadabort', reject);

    // |setUserAgentOverride| should automatically reload.
    controlledframe.setUserAgentOverride(userAgent);
  });
}

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');

  const iwaUserAgent = navigator.userAgent;

  let cfUserAgent = await getControlledFrameUserAgent(controlledFrame);
  assert_false(controlledFrame.isUserAgentOverridden());
  assert_true(cfUserAgent === iwaUserAgent);

  await setUserAgentOverrideAndAwaitReload(controlledFrame, 'foobar');
  assert_true(controlledFrame.isUserAgentOverridden());
  cfUserAgent = await getControlledFrameUserAgent(controlledFrame);
  assert_true(cfUserAgent === 'foobar');

  await setUserAgentOverrideAndAwaitReload(controlledFrame, '');
  assert_false(controlledFrame.isUserAgentOverridden());
  cfUserAgent = await getControlledFrameUserAgent(controlledFrame);
  assert_true(cfUserAgent === iwaUserAgent);
}, 'User Agent Override');
