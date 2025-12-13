// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns whether the current platform is Android.
async function isAndroid() {
  const os = await new Promise((resolve) => {
    chrome.runtime.getPlatformInfo(info => resolve(info.os));
  });
  return os === 'android';
}

chrome.test.runTests([
  function typeNormal() {
    chrome.windows.create({'type': 'normal'}, chrome.test.callbackPass(w => {
      chrome.test.assertEq('normal', w.type);
    }));
  },
  async function typePopup() {
    if (await isAndroid()) {
      // TODO(https://crbug.com/431004500): Enable this test on android.
      chrome.test.succeed();
      return;
    }

    const w = await new Promise((resolve) => {
        chrome.windows.create({'type': 'popup'}, resolve);
    });
    chrome.test.assertEq('popup', w.type);
    chrome.test.succeed();
  },
  function sizeTooBig() {
    // Setting origin + bad width/height should not crash.
    chrome.windows.create({
      'type': 'normal',
      'left': 0,
      'top': 0,
      'width': 2147483647,
      'height': 2147483647,
    }, (w => {
      chrome.test.assertLastError('Invalid value for bounds. Bounds must be ' +
                                  'at least 50% within visible screen space.');
      chrome.test.succeed();
    }));
  },
]);
