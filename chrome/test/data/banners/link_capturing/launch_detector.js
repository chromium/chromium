// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var launchParamsTargetUrls = [];

var launchParamsReceived = null;
var resolveLaunchParamsReceived = null;

window.launchQueue.setConsumer((launchParams) => {
  console.assert('targetURL' in launchParams);
  launchParamsTargetUrls.push(launchParams.targetURL);
  if (launchParamsReceived != null) {
    resolveLaunchParamsReceived();
    launchParamsReceived = null;
    resolveLaunchParamsReceived = null;
  }
});

async function listenForNextLaunchParams(delay) {
  // Assert we aren't currently listening for a launch.
  console.assert(
      launchParamsReceived == null && resolveLaunchParamsReceived == null);
  launchParamsReceived = new Promise(function(resolve) {
    resolveLaunchParamsReceived = resolve;
  });
  await launchParamsReceived;
  // Ensure the promise was cleaned up.
  console.assert(
      launchParamsReceived == null && resolveLaunchParamsReceived == null);
  signalNavigationComplete(
      delay, launchParamsTargetUrls[launchParamsTargetUrls.length - 1]);
}

function signalNavigationComplete(delay, forLaunchUrl) {
  forLaunchUrl = forLaunchUrl || '';
  if (forLaunchUrl.length != 0) {
    forLaunchUrl =
        ' for launchParams.targetUrl: ' + (new URL(forLaunchUrl)).pathname;
  }
  listenForNextLaunchParams(delay);

  const doneMessage = 'FinishedNavigating in ' +
      (window.frameElement === null ? 'frame' : 'iframe') + ' for url ' +
      window.location.pathname + forLaunchUrl;
  // To facilitate receiving launchParams as that can be async,
  // always use setTimeout().
  setTimeout(() => {
    if (delay != 0) {
      console.log('Delay end for a bit (' + delay + ')');
    }
    domAutomationController.send(doneMessage);
  }, delay);
}

function showParams(params) {
  let output = decodeURI(params);
  console.log('Query params passed in: ' + output);
  // Make the query params easier to read on the page.
  output = output.replace('?', '\n');
  output = output.replace('&', '\n');
  document.getElementById('debug').textContent = 'Params: ' + output;
}
