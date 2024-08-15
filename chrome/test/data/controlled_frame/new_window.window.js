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

  const popupLoadedPromise = new Promise((resolve, reject) => {
    controlledFrame.addEventListener('newwindow', (e) => {
      const popupControlledFrame = document.createElement('controlledframe');
      // Attach the new window to the new <controlledframe>.
      popupControlledFrame.addEventListener(
          'loadstop', resolve.bind(null, popupControlledFrame));
      popupControlledFrame.addEventListener('loadabort', reject);
      e.window.attach(popupControlledFrame);
      document.body.appendChild(popupControlledFrame);
    });
  });
  controlledFrame.executeScript({code: 'window.open("/title2.html");'});
  const popupControlledFrame = await popupLoadedPromise;

  assert_equals(
      await executeScript(controlledFrame, 'location.pathname'),
      '/simple.html');
  assert_equals(
      await executeScript(popupControlledFrame, 'location.pathname'),
      '/title2.html');
}, "New Window event");
