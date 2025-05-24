// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Saves all launch urls consumed by this document. This is fetched by the tests
// in web_app_link_capturing_parameterized_browsertest.cc to determine all of
// the launches that occurred for this document.
var launchParamsTargetUrls = [];

var _onLaunchQueueFlushedByBrowser = null;

// This function is called by the c++ test fixture to signify that the launch
// queue mojom receiver was flushed. This can be called at any point in the
// page's lifecycle, as the test currently has to spam-call this to all web
// contents when it receives a "PleaseFlushLaunchQueue" message.
function resolveLaunchParamsFlush() {
  if (_onLaunchQueueFlushedByBrowser != null) {
    _onLaunchQueueFlushedByBrowser();
    _onLaunchQueueFlushedByBrowser = null;
  }
  // Forward call to frames, as the navigation could have happened in a frame.
  const frames = window.frames;
  for (let i = 0; i < frames.length; i++) {
    if ('resolveLaunchParamsFlush' in frames[i]) {
      frames[i].resolveLaunchParamsFlush();
    }
  }
}

var _onLaunchParamsReceived = null;
var _launchParamToBeSentToBrowser = null;

window.launchQueue.setConsumer((launchParams) => {
  console.assert('targetURL' in launchParams);
  console.log('Got launch at ' + launchParams.targetURL);
  console.assert(
      _launchParamToBeSentToBrowser == null,
      'Launch params from the last launch have not ' +
          'been sent to the browser test fixture.');
  _launchParamToBeSentToBrowser = launchParams.targetURL;
  launchParamsTargetUrls.push(launchParams.targetURL);
  if (_onLaunchParamsReceived != null) {
    _onLaunchParamsReceived();
    _onLaunchParamsReceived = null;
  }
});

async function _listenForNextLaunchParams(delay) {
  console.log('Listening for next launch in ' + window.location.pathname);
  console.assert(
      _onLaunchParamsReceived == null,
      'Cannot listen for two launches at the same time.');
  let waitingForLaunchParamsPromise = new Promise(function(resolve) {
    _onLaunchParamsReceived = resolve;
  });
  await waitingForLaunchParamsPromise;
  console.assert(_onLaunchParamsReceived == null);
  _signalNavigationCompleteAndListenForNextLaunch(delay);
}

// Sends the "PleaseFlushLaunchQueue" message, waits for
// `resolveLaunchParamsFlush()` to be called and then sends the
// 'FinishedNavigating' message. This dance is required because the launch queue
// is separate from page load, and sometimes it can be sent after the 'load'
// event is sent (which is when this function is called). Thus the two messages
// happen here, and the browser calls `resolveLaunchParamsFlush()` after it
// flushes all launch queues, ensuring that the launch params will have gotten
// here before this function finally sends "FinishedNavigating".
function _signalNavigationCompleteAndListenForNextLaunch(delay) {
  console.assert(
      _onLaunchQueueFlushedByBrowser == null,
      'Cannot signal navigation completion until ' +
          'the existing call has completed.');
  let launchQueueFlushed = new Promise((resolve) => {
    _onLaunchQueueFlushedByBrowser = resolve;
  });
  setTimeout(async () => {
    console.assert(_onLaunchQueueFlushedByBrowser != null);
    const FLUSH_PREFIX = 'PleaseFlushLaunchQueue';
    const FINISHED_NAVIGATING_PREFIX = 'FinishedNavigating';
    let messageSuffix = ' in ' +
        (window.frameElement === null ? 'frame' : 'iframe') + ' for url ' +
        window.location.pathname;


    console.log(FLUSH_PREFIX + messageSuffix);
    if (typeof domAutomationController !== 'undefined') {
      domAutomationController.send(FLUSH_PREFIX + messageSuffix);
    }
    await launchQueueFlushed;
    console.assert(_onLaunchQueueFlushedByBrowser == null);

    if (_launchParamToBeSentToBrowser != null) {
      messageSuffix += ' for launchParams.targetUrl: ' +
          (new URL(_launchParamToBeSentToBrowser)).pathname;
      _launchParamToBeSentToBrowser = null;
    }

    console.log(FINISHED_NAVIGATING_PREFIX + messageSuffix);

    if (typeof domAutomationController !== 'undefined') {
      domAutomationController.send(FINISHED_NAVIGATING_PREFIX + messageSuffix);
    }

    // Listen for the next launch param in order to handle 'navigate-existing'
    // and 'focus-existing' client modes.
    _listenForNextLaunchParams(delay);
  }, delay);
}

function _showParams(params) {
  let output = decodeURI(params);
  console.log('Query params passed in: ' + output);
  // Make the query params easier to read on the page.
  output = output.replace('?', '\n');
  output = output.replace('&', '\n');
  document.getElementById('debug').textContent = 'Params: ' + output;
}
