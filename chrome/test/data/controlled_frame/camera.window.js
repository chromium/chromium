// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

promise_setup(async () => {
  await test_driver.set_permission({ name: "camera" }, "granted");
});

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
  let permissionRequestHandled = false;
  controlledFrame.addEventListener('permissionrequest', (e) => {
    if (e.permission === 'media') {
      permissionRequestHandled = true;
      e.request.allow();
    } else {
      e.request.deny();
    }
  });

  const testScript = `(async () => {
    const mediaStream =
        await navigator.mediaDevices.getUserMedia({video: true});
    return mediaStream.getVideoTracks().length > 0;
  })()`;
  const hasVideo = await executeAsyncScript(controlledFrame, testScript);

  assert_true(permissionRequestHandled);
  assert_true(hasVideo);
}, 'Camera permission');
