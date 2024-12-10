// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

// Listing all the event handlers:
const EventHandlers = [
  // For Type 1 - 'addEventListener(EVENT, FUNC)'
  {
    registerFunc: (controlledframe, name, eventHandler) => {
      controlledframe.addEventListener(name, eventHandler);
    },
    unregisterFunc: (controlledframe, name, eventHandler) => {
      controlledframe.removeEventListener(name, eventHandler);
    }
  },
  // For Type 2 - 'onEVENT = FUNC'
  {
    registerFunc: (controlledframe, name, eventHandler) => {
      controlledframe['on' + name] = eventHandler;
    },
    unregisterFunc: (controlledframe, name, eventHandler) => {
      controlledframe['on' + name] = null;
    }
  }
];

// Listing all the events.
// Each event object has the following properties:
// name:              The name of the event.
// trigger:           A function that triggers the event.
// optionalCallback:  An optional callback function that will be called when the
// event is triggered.
const AllEvents = [
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
  },
  {
    name: 'newwindow',
    trigger: async (controlledframe) => {
      controlledframe.executeScript({code: 'window.open("/title2.html");'});
      // Wait a short time for window to be dropped.
      await new Promise((resolve) => setTimeout(resolve, 100));
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

  for (oneEvent of AllEvents) {
    for (eventHandler of EventHandlers) {
      let counter = 0;
      const recordAndMaybeRunCallback = function(...args) {
        counter++;
        if (typeof oneEvent.optionalCallback === 'function') {
          oneEvent.optionalCallback(...args);
        }
      };

      // Register the handler.
      eventHandler.registerFunc(
          controlledFrame, oneEvent.name, recordAndMaybeRunCallback);

      // Trigger the event, and verify that counter is increased.
      await oneEvent.trigger(controlledFrame);
      assert_true(
          counter === 1,
          `Expected ${
              oneEvent.name} to be triggered 1 time, but actually triggered ${
              counter} time(s).`);

      // Reset the counter and unregister the handler.
      counter = 0;
      eventHandler.unregisterFunc(
          controlledFrame, oneEvent.name, recordAndMaybeRunCallback);

      // Trigger the event again. Observe that the counter is not changed.
      await oneEvent.trigger(controlledFrame);
      assert_true(
          counter === 0,
          `Expected ${
              oneEvent.name} to be triggered 0 time, but actually triggered ${
              counter} time(s).`);

      if (oneEvent.resetControlledFrameAfter) {
        controlledFrame.remove();
        controlledFrame = await createControlledFrame('/simple.html');
      }
    }
  }
}, 'Event Handlers');
