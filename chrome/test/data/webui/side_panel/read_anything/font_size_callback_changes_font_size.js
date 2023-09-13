// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.FontSizeCallback_ChangesFontSize

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const toolbar =
      readAnythingApp.querySelector('read-anything-toolbar').shadowRoot;
  const container = readAnythingApp.getElementById('container');
  const startingFontSize =
      parseInt(window.getComputedStyle(container).fontSize);

  let result = true;

  const assertGT = (greater, less) => {
    const isGreater = parseInt(greater) > parseInt(less);
    if (!isGreater) {
      console.error(
          'Expected ' + JSON.stringify(greater) + ' to be greater than ' +
          +JSON.stringify(less));
    }
    result = result && isGreater;
    return isGreater;
  };

  const assertEquals = (actual, expected) => {
    const isEqual = actual === expected;
    if (!isEqual) {
      console.error(
          'Expected: ' + JSON.stringify(expected) + ', ' +
          'Actual: ' + JSON.stringify(actual));
    }
    result = result && isEqual;
    return isEqual;
  };

  toolbar.getElementById('font-size-increase').click();
  assertGT(
      parseInt(window.getComputedStyle(container).fontSize), startingFontSize);

  toolbar.getElementById('font-size-decrease').click();
  toolbar.getElementById('font-size-decrease').click();
  assertGT(
      startingFontSize, parseInt(window.getComputedStyle(container).fontSize));

  toolbar.getElementById('font-size-reset').click();
  assertEquals(
      startingFontSize, parseInt(window.getComputedStyle(container).fontSize));

  return result;
})();
