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
    trigger: async (controlledrame) => {
      const triggerScript = `console.log('foobar')`;
      await executeAsyncScript(controlledrame, triggerScript);
    }
  },
  {
    name: 'permissionrequest',
    trigger: async (controlledrame) => {
      const triggerScript = `(async () => {
        await new Promise((resolve, reject) =>
            navigator.geolocation.getCurrentPosition(resolve, resolve));
      })()`;
      await executeAsyncScript(controlledrame, triggerScript);
    }
  }
];


promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
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
      assert_true(counter === 1);

      // Reset the counter and unregister the handler.
      counter = 0;
      eventHandler.unregisterFunc(
          controlledFrame, oneEvent.name, recordAndMaybeRunCallback);

      // Trigger the event again. Observe that the counter is not changed.
      await oneEvent.trigger(controlledFrame);
      assert_true(counter === 0);
    }
  }
}, 'Event Handlers');
