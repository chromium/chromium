// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function createControlledFrame(path) {
  const params = new URLSearchParams(location.search);
  if (!params.has('https_origin')) {
    throw new Exception('No https_origin query parameter provided');
  }
  const url = new URL(path, params.get('https_origin')).toString();

  const cf = document.createElement('controlledframe');
  await new Promise((resolve, reject) => {
    cf.addEventListener('loadstop', resolve);
    cf.addEventListener('loadabort', reject);
    cf.setAttribute('src', url);
    document.body.appendChild(cf);
  });
  return cf;
}

async function executeScript(controlledFrame, script) {
  // We should switch to 'await' when executeScript supports promises:
  // const results = await controlledFrame.executeScript({code: script});
  // if (results.length !== 1) {
  //   throw new Error('Expected 1 result from executeScript: ' + script);
  // } else {
  //   return results[0];
  // }
  return new Promise((resolve, reject) => {
    controlledFrame.executeScript({code: script}, (results) => {
      if (results.length !== 1) {
        reject('Expected 1 result from executeScript: ' + script);
      } else {
        resolve(results[0]);
      }
    });
  });
}

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
  controlledFrame.addEventListener('permissionrequest', (e) => {
    if (e.permission === 'geolocation') {
      e.request.allow();
    } else {
      e.request.deny();
    }
  });

  const position = await executeScript(
      controlledFrame,
      'new Promise((resolve, reject) => ' +
          'navigator.geolocation.getCurrentPosition(resolve, reject))');
  assert_true('coords' in position);
}, "Gelocation permission");
