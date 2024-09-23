// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');

  const popupLoadedPromise = new Promise((resolve, reject) => {
    controlledFrame.addEventListener('newwindow', (e) => {
      const popupControlledFrame = document.createElement('controlledframe');
      // Attach the new window to the new <controlledframe>.
      popupControlledFrame.addEventListener(
          'loadstop', resolve.bind(null, popupControlledFrame));
      popupControlledFrame.addEventListener('loadabort', reject);
      e.window.attach(popupControlledFrame);
      document.body.appendChild(popupControlledFrame);
    });
  });
  controlledFrame.executeScript({code: 'window.open("/title2.html");'});
  const popupControlledFrame = await popupLoadedPromise;

  assert_equals(
      await executeAsyncScript(controlledFrame, 'location.pathname'),
      '/simple.html');
  assert_equals(
      await executeAsyncScript(popupControlledFrame, 'location.pathname'),
      '/title2.html');
}, "New Window event");
