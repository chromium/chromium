// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateTheme_FontName

const readAnythingApp = document.querySelector('read-anything-app').shadowRoot;
const container = readAnythingApp.getElementById('container');
let result = true;

function assertEquals(actual, expected) {
  const isEqual = actual === expected;
  if (!isEqual) {
    console.error(
        'Expected: ' + JSON.stringify(expected) + ', ' +
        'Actual: ' + JSON.stringify(actual));
  }
  result = result && isEqual;
  return isEqual;
}

function assertFontName(expected) {
  assertEquals(expected, getComputedStyle(container).fontFamily);
}

chrome.readAnything.setThemeForTesting('Standard font', 18.0, 0, 0, 1, 0);
assertFontName('"Standard font"');

chrome.readAnything.setThemeForTesting('Sans-serif', 18.0, 0, 0, 1, 0);
assertFontName('sans-serif');

chrome.readAnything.setThemeForTesting('Serif', 18.0, 0, 0, 1, 0);
assertFontName('serif');

chrome.readAnything.setThemeForTesting('Arial', 18.0, 0, 0, 1, 0);
assertFontName('Arial');

chrome.readAnything.setThemeForTesting('Comic Sans MS', 18.0, 0, 0, 1, 0);
assertFontName('"Comic Sans MS"');

chrome.readAnything.setThemeForTesting('Times New Roman', 18.0, 0, 0, 1, 0);
assertFontName('"Times New Roman"');

domAutomationController.send(result);
