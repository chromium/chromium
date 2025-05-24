// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js
// META script=resources/event_handler_helpers.js

// Listing part 1/2 of all the frame events. Frame events are events directly
// associated with <controlledframe> and not part of WebRequest or ContextMenus
// API. These events are split into 2 tests to reduce test flakiness due to
// timeout. Each event object has the following properties:
// name: The name of the event.
// trigger: A function that triggers the event.
// optionalCallback:  An optional callback function that will be called when the
// event is triggered.
const FrameEventsPart1 = [
  {
    name: 'consolemessage',
    trigger: async (controlledframe) => {
      const triggerScript = `console.log('foobar')`;
      await executeAsyncScript(controlledframe, triggerScript);
    }
  },
  {
    name: 'contentload',
    trigger: async (controlledframe) => {
      await new Promise((resolve, reject) => {
        controlledframe.addEventListener('loadstop', resolve, {once: true});
        controlledframe.reload();
      });
    }
  },
  {
    name: 'dialog',
    trigger: async (controlledframe) => {
      const triggerScript = `window.confirm('hello world');`;
      await executeAsyncScript(controlledframe, triggerScript);
    }
  },
  {
    name: 'loadabort',
    trigger: async (controlledframe) => {
      await new Promise((resolve, reject) => {
        controlledframe.addEventListener('loadabort', resolve, {once: true});
        controlledframe.src = 'chrome://flags';
      });
    },
    // Resets the <controlledframe> because this test case changes the 'src'
    // attribute.
    resetControlledFrameAfter: true
  },
  {
    name: 'loadcommit',
    trigger: async (controlledframe) => {
      await new Promise((resolve, reject) => {
        controlledframe.addEventListener('loadstop', resolve, {once: true});
        controlledframe.reload();
      });
    }
  },
  {
    name: 'loadstart',
    trigger: async (controlledframe) => {
      await new Promise((resolve, reject) => {
        controlledframe.addEventListener('loadstop', resolve, {once: true});
        controlledframe.reload();
      });
    }
  },
  {
    name: 'loadstop',
    trigger: async (controlledframe) => {
      await new Promise((resolve, reject) => {
        controlledframe.addEventListener('loadstop', resolve, {once: true});
        controlledframe.reload();
      });
    }
  }
];

promise_test(async (test) => {
  let controlledFrame = await createControlledFrame('/simple.html');
  await testEventHanders(controlledFrame, FrameEventsPart1);
}, 'Event Handlers');
