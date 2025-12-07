// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

async function testEventHanders(controlledFrame, events) {
  for (oneEvent of events) {
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
};

// The list of WebRequest events.
const WebRequestEvents = [
  'authrequired',
  'beforeredirect',
  'beforerequest',
  'beforesendheaders',
  'completed',
  'headersreceived',
  'responsestarted',
  'sendheaders',
];

function addWebRequestListeners(controlledframe, targetUrl) {
  window.events = [];

  const interceptor = controlledframe.request.createWebRequestInterceptor({
    urlPatterns: [targetUrl],
    resourceTypes: ['main-frame', 'xmlhttprequest'],
  });
  for (const eventName of WebRequestEvents) {
    interceptor.addEventListener(eventName, (e) => {
      window.events.push(eventName);
    });
  }

  // Temporarily add special case for onErrorOccurred here to monitor it being
  // triggered in the CI.
  // TODO(crbug.com/386380410): clean up.
  window.occurredErrors = [];
  interceptor.addEventListener('erroroccurred', (e) => {
    window.events.push('erroroccurred');
    window.occurredErrors.push(e.error);
  });
}

function verifyWebRequestEvents(expected) {
  // Temporarily add special case for onErrorOccurred here to monitor it being
  // triggered in the CI.
  // TODO(crbug.com/386380410): clean up.
  if (window.occurredErrors.length > 0) {
    console.log(`onErrorOccurred triggered ${
        window.occurredErrors.length} times, errors are:\n${
        window.occurredErrors
            .map((item, index) => `${index + 1}. ${item}`)
            .join('\n')}\n`);
  }

  assert_equals(JSON.stringify(window.events), JSON.stringify(expected));
}
