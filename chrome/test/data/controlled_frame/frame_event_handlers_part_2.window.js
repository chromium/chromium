// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js
// META script=resources/event_handler_helpers.js

// Listing part 2/2 of all the frame events. Frame events are events directly
// associated with <controlledframe> and not part of WebRequest or ContextMenus
// API. These events are split into 2 tests to reduce test flakiness due to
// timeout. Each event object has the following properties:
// name: The name of the event.
// trigger: A function that triggers the event.
// optionalCallback:  An optional callback function that will be called when the
// event is triggered.
const FrameEventsPart2 = [
  {
    name: 'newwindow',
    trigger: async (controlledframe) => {
      controlledframe.executeScript({code: 'window.open("/title2.html");'});
      // Wait a short time for window to be dropped.
      await new Promise((resolve) => setTimeout(resolve, 500));
    }
  },
  {
    name: 'permissionrequest',
    trigger: async (controlledframe) => {
      const triggerScript = `(async () => {
        await new Promise((resolve, reject) =>
            navigator.geolocation.getCurrentPosition(resolve, resolve));
      })()`;
      await executeAsyncScript(controlledframe, triggerScript);
    }
  },
  {
    name: 'zoomchange',
    trigger: async (controlledframe) => {
      controlledframe.setZoom(0.25325);
      // Wait a short time for zoom to apply.
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
  }
];

promise_test(async (test) => {
  let controlledFrame = await createControlledFrame('/simple.html');
  await testEventHanders(controlledFrame, FrameEventsPart2);
}, 'Event Handlers');
