// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

// This test expects WebRequest events to be triggered for a basic GET request
// inside Controlled Frame. 'onAuthRequired', 'onBeforeRedirect',
// 'onErrorOccurred' are expected to not trigger.

// The list of WebRequest events.
const WebRequestEvents = [
  {name: 'onAuthRequired', shouldTrigger: false},
  {name: 'onBeforeRedirect', shouldTrigger: false},
  {name: 'onBeforeRequest', shouldTrigger: true},
  {name: 'onBeforeSendHeaders', shouldTrigger: true},
  {name: 'onCompleted', shouldTrigger: true},
  {name: 'onHeadersReceived', shouldTrigger: true},
  {name: 'onHeadersReceived', shouldTrigger: true},
  {name: 'onSendHeaders', shouldTrigger: true},
];

promise_test(async (test) => {
  const controlledframe = await createControlledFrame('/simple.html');
  controlledframe.stop();

  const targetUrl = new URL(controlledframe.src);
  targetUrl.pathname = '/handbag.png';

  for (const singleEvent of WebRequestEvents) {
    const eventName = singleEvent.name;
    window['count' + eventName] = 0;
    const eventCounter = function() {
      window['count' + this.name] += 1;
    };

    controlledframe.request[eventName].addListener(
        eventCounter.bind({name: eventName}), {urls: [targetUrl.toString()]});
  }

  // Temporarily add special case for onErrorOccurred here to monitor it being
  // triggered in the CI.
  // TODO(crbug.com/386380410): clean up.
  window['occurredErrors'] = [];
  controlledframe.request.onErrorOccurred.addListener(function(details) {
    window['occurredErrors'].push(details.error);
  }, {urls: [targetUrl.toString()]});

  const script = `(async() => {
      const response = await fetch('${targetUrl.toString()}', {method:'GET'});
      await response.blob();
      })();`;
  await executeAsyncScript(controlledframe, script);

  // Temporarily add special case for onErrorOccurred here to monitor it being
  // triggered in the CI.
  // TODO(crbug.com/386380410): clean up.
  assert_true(
      window['occurredErrors'].length === 0,
      console.log(`\nonErrorOccurred triggered ${
          window['occurredErrors'].length} times, errors are:\n${
          window['occurredErrors']
              .map((item, index) => `${index + 1}. ${item}`)
              .join('\n')}\n`));

  for (singleEvent of WebRequestEvents) {
    const eventName = singleEvent.name;
    console.log(
        `${eventName} triggered ${window['count' + eventName]} time(s).`);

    if (singleEvent.shouldTrigger) {
      assert_true(
          window['count' + eventName] > 0,
          `Expected ${eventName} to trigger more than 0 times, but triggered ${
              window['count' + eventName]} time(s).`);
    } else {
      assert_true(
          window['count' + eventName] == 0,
          `Expected ${eventName} to trigger 0 times, but triggered ${
              window['count' + eventName]} time(s).`);
    }
  }
}, 'WebRequest Event Handlers');
