// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateTheme_BackgroundColor

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const container = readAnythingApp.getElementById('container');

  chrome.readingMode.setThemeForTesting(
      'f', 1, 0, /* SkColorSetRGB(0xFD, 0xE2, 0x93) = */ 4294828691, 1, 0);
  const expected = 'rgb(253, 226, 147)';  // #FDE293
  const actual = getComputedStyle(container).backgroundColor;
  const isEqual = actual === expected;
  if (!isEqual) {
    console.error(
        'Expected: ' + JSON.stringify(expected) + ', ' +
        'Actual: ' + JSON.stringify(actual));
  }
  return isEqual;
})();
