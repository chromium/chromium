// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.SupportedFonts_Correct

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(async () => {
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

  const assertFontForLanguageCode = (code, expected) => {
    chrome.readingMode.setLanguageForTesting(code);
    const dropdown = toolbar.getElementById('fontMenu');
    const buttons = Array.from(dropdown.querySelectorAll('.dropdown-item'));
    assertEquals(buttons.length, expected);
  };

  assertFontForLanguageCode('en', 8);
  assertFontForLanguageCode('es', 8);
  assertFontForLanguageCode('zz', 2);
  assertFontForLanguageCode('hi', 3);
  assertFontForLanguageCode('tr', 7);

  return result;
})();
