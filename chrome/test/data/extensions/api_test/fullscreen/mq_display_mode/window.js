// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Resolves once `(display-mode: <mode>)` matches. A resize event alone is
// not enough: a late resize from the previous transition can arrive after
// the next one was requested, while the old display mode is still in effect.
function waitForDisplayMode(mode) {
  return new Promise(resolve => {
    const query = matchMedia(`(display-mode: ${mode})`);
    if (query.matches) {
      resolve();
      return;
    }
    query.addEventListener('change', function listener() {
      if (query.matches) {
        query.removeEventListener('change', listener);
        resolve();
      }
    });
  });
}

window.onload = function() {
  chrome.test.runTests([async function checkDisplayModeMediaFeature() {
    chrome.test.assertTrue(
        matchMedia('(display-mode: standalone)').matches,
        `Initially display mode is 'standalone'`);

    chrome.app.window.current().fullscreen();
    await waitForDisplayMode('fullscreen');

    chrome.app.window.current().restore();
    await waitForDisplayMode('standalone');

    chrome.test.succeed();
  }]);
};
