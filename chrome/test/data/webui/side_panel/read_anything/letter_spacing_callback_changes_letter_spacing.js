// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.LetterSpacingCallback_ChangesLetterSpacing

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
  const letterSpacingMenu = toolbar.getElementById('letterSpacingMenu');

  let result = true;

  const assertNE = (actual, expected) => {
    const isNotEqual = actual !== expected;
    if (!isNotEqual) {
      console.error(
          'Expected ' + JSON.stringify(actual) + ' to be not equal to ' +
          +JSON.stringify(expected));
    }
    result = result && isNotEqual;
    return isNotEqual;
  };

  const buttons =
      Array.from(letterSpacingMenu.querySelectorAll('.dropdown-item'));
  let previousLetterSpacing = -1;
  buttons.forEach((button) => {
    button.click();
    const newLetterSpacing = (window.getComputedStyle(container).letterSpacing);
    assertNE(newLetterSpacing, previousLetterSpacing);
    previousLetterSpacing = newLetterSpacing;
  });

  return result;
})();
