// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateTheme_FontName

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const container = readAnythingApp.getElementById('container');
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

  const assertFontName = (expected) => {
    assertEquals(expected, getComputedStyle(container).fontFamily);
  };

  chrome.readingMode.setThemeForTesting('Poppins', 18.0, 0, 0, 1, 0);
  assertFontName('Poppins');

  chrome.readingMode.setThemeForTesting('Sans-serif', 18.0, 0, 0, 1, 0);
  assertFontName('sans-serif');

  chrome.readingMode.setThemeForTesting('Serif', 18.0, 0, 0, 1, 0);
  assertFontName('serif');

  chrome.readingMode.setThemeForTesting('Comic Neue', 18.0, 0, 0, 1, 0);
  assertFontName('"Comic Neue"');

  chrome.readingMode.setThemeForTesting('Lexend Deca', 18.0, 0, 0, 1, 0);
  assertFontName('"Lexend Deca"');

  chrome.readingMode.setThemeForTesting('EB Garamond', 18.0, 0, 0, 1, 0);
  assertFontName('"EB Garamond"');

  chrome.readingMode.setThemeForTesting('STIX Two Text', 18.0, 0, 0, 1, 0);
  assertFontName('"STIX Two Text"');

  return result;
})();
