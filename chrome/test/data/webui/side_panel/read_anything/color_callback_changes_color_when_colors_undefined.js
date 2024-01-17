// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.ColorCallback
//       _ChangesColorWhenColorsUndefined

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp = document.querySelector('read-anything-app');
  const toolbar =
      readAnythingApp.shadowRoot.querySelector('read-anything-toolbar')
          .shadowRoot;
  const container = readAnythingApp.shadowRoot.getElementById('container');
  const colorMenu = toolbar.getElementById('colorMenu');

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

  const assertEQ = (actual, expected) => {
    const isEqual = actual === expected;
    if (!isEqual) {
      console.error(
          'Expected ' + JSON.stringify(actual) + ' to be equal to ' +
          +JSON.stringify(expected));
    }
    result = result && isEqual;
    return isEqual;
  };

  // On certain platforms, sometimes in tests the color tokens are actually
  // available. When this happens, we don't need to execute the rest of the
  // test because this behavior is tested in color_callback_changes_color.js
  // when areColorTokensUnavailable always returns false to simulate color
  // tokens being available. areColorTokensUnavailable returning false naturally
  // in tests doesn't happen consistently, so getting rid of this early return
  // can result in flaky tests.
  // Alternatively, we could set areColorTokensUnavailable to always return
  // true in this test, but since that obscures the actual check in
  // areColorTokensUnavailable, this approach is preferable.
  if (!readAnythingApp.areColorTokensUnavailable()) {
    return result;
  }

  const buttons = Array.from(colorMenu.querySelectorAll('.dropdown-item'));
  buttons.forEach((button) => {
    button.click();
    let previousBackground = '';
    const newBackground = (window.getComputedStyle(container).backgroundColor);
    const newForeground = (window.getComputedStyle(container).color);

    // Ensure that the colors are defined as the same colors defined in app.ts.
    if (button.textContent.includes('Dark')) {
      assertEQ(newForeground.includes('rgb(227, 227, 227'), true);
    } else {
      assertEQ(newForeground.includes('rgb(31, 31, 31'), true);
    }

    // Since the default color can be the same as other colors, testing if the
    // color changed won't work for testing Default. Therefore, exclude it
    // from testing if it differs from the previous color.
    if (!button.textContent.includes('Default')) {
      assertNE(newBackground, previousBackground);
    }

    previousBackground = '';
  });

  return result;
})();
