// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Saves all launch urls consumed by this document. This is fetched by the tests
// in web_app_link_capturing_parameterized_browsertest.cc to determine all of
// the launches that occurred for this document.
var launchParamsTargetUrls = [];

// The most recent launch url sent that hasn't been sent to the test framework
// via the domAutomationController.
var unsentLaunchParamUrl = null;

// This promise is used by `listenForNextLaunchParams` to wait for the next
// launch param.
var waitingForLaunchParamsPromise = null;
var resolveLaunchParamsPromise = null;

window.launchQueue.setConsumer((launchParams) => {
  console.assert('targetURL' in launchParams);
  console.log('Got launch at ' + launchParams.targetURL);
  // This test page is designed to try to not ever have a launch
  // params that wasn't sent back to the `domAutomationController` before
  // the next one arrives, so assert-fail if this happens.
  console.assert(unsentLaunchParamUrl == null);
  unsentLaunchParamUrl = launchParams.targetURL;
  launchParamsTargetUrls.push(launchParams.targetURL);
  if (waitingForLaunchParamsPromise != null) {
    resolveLaunchParamsPromise();
    waitingForLaunchParamsPromise = null;
    resolveLaunchParamsPromise = null;
  }
});

async function listenForNextLaunchParams(delay) {
  console.log('Listening for next launch in ' + window.location.pathname);
  // It is invalid to listen multiple times overlapping.
  console.assert(
      waitingForLaunchParamsPromise == null &&
      resolveLaunchParamsPromise == null);
  waitingForLaunchParamsPromise = new Promise(function(resolve) {
    resolveLaunchParamsPromise = resolve;
  });
  await waitingForLaunchParamsPromise;
  // Ensure the promise was cleaned up.
  console.assert(
      waitingForLaunchParamsPromise == null &&
      resolveLaunchParamsPromise == null);
  signalNavigationCompleteAndListenForNextLaunch(delay);
}

// Sends the 'FinishedNavigating' message to `the domAutomationController`.
// TODO(crbug.com/371180649): Include debug message in this message after
// 'string contains' functionality is added to the Kombucha tests.
function signalNavigationCompleteAndListenForNextLaunch(delay) {
  // Always set timeout, which also helps with trying to catch any launch params
  // that haven't hit the consumer.
  setTimeout(() => {
    let message = 'FinishedNavigating in ' +
        (window.frameElement === null ? 'frame' : 'iframe') + ' for url ' +
        window.location.pathname;
    // Note: We can assume in Chromium that the launch params will be consumed
    // before the 'load' event is dispatched as long as there is a consumer set
    // on the window.
    if (unsentLaunchParamUrl != null) {
      message += ' for launchParams.targetUrl: ' +
          (new URL(unsentLaunchParamUrl)).pathname;
      unsentLaunchParamUrl = null;
    }
    console.log(message);
    domAutomationController.send('FinishedNavigating');

    // Listen for the next launch param (which, in our tests, should occur in
    // the 'focus-existing' client mode).
    listenForNextLaunchParams(delay);
  }, delay)
}

function showParams(params) {
  let output = decodeURI(params);
  console.log('Query params passed in: ' + output);
  // Make the query params easier to read on the page.
  output = output.replace('?', '\n');
  output = output.replace('&', '\n');
  document.getElementById('debug').textContent = 'Params: ' + output;
}
