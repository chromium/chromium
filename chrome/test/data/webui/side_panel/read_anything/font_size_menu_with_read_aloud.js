// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.ReadAloud_FontSizeMenu

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const toolbar =
      readAnythingApp.querySelector('read-anything-toolbar').shadowRoot;

  let result = true;
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
  const assertNE = (actual, expected) => {
    const isNotEqual = actual !== expected;
    if (!isNotEqual) {
      console.error(
          'Expected ' + JSON.stringify(actual) + ' to be not equal to ' +
          JSON.stringify(expected));
    }
    result = result && isNotEqual;
    return isNotEqual;
  };

  const font_size_menu_button = toolbar.getElementById('font-size');
  assertNE(font_size_menu_button, null);
  font_size_menu_button.click();
  const font_size_menu = toolbar.getElementById('fontSizeMenu');
  assertEquals(font_size_menu.open, true);

  const font_size_increase_button =
      toolbar.getElementById('font-size-increase-old');
  assertEquals(font_size_increase_button, null);
  const font_size_decrease_button =
      toolbar.getElementById('font-size-decrease-old');
  assertEquals(font_size_decrease_button, null);

  return result;
})();
