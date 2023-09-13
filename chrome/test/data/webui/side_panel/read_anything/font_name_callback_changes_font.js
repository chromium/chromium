// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.FontNameCallback_ChangesFont

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
  const fontMenu = toolbar.getElementById('fontMenu');

  let result = true;
  const assertEquals = (actual, expected) => {
    // Some of the font families from the computed style contain quotes
    const quote = RegExp('"', 'g');
    const isEqual = actual.trim().toLowerCase().replace(quote, '') ===
        expected.trim().toLowerCase().replace(quote, '');
    if (!isEqual) {
      console.error(
          'Expected: ' + JSON.stringify(expected) + ', ' +
          'Actual: ' + JSON.stringify(actual));
    }
    result = result && isEqual;
    return isEqual;
  };

  const buttons = Array.from(fontMenu.querySelectorAll('.dropdown-item'));
  buttons.forEach((button) => {
    button.click();
    assertEquals(
        window.getComputedStyle(container).fontFamily, button.textContent);
  });

  return result;
})();
