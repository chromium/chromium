// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.UpdateTheme_LineSpacing

const readAnythingApp = document.querySelector('read-anything-app').shadowRoot;
const container = readAnythingApp.getElementById('container');

chrome.readAnything.setThemeForTesting('Standard font', 1.0, 0, 0, 2, 0);
const expected = '24px';  // 1.5 times the 1em (16px) font size
const actual = getComputedStyle(container).lineHeight;
const isEqual = actual === expected;
if (!isEqual) {
  console.error(
      'Expected: ' + JSON.stringify(expected) + ', ' +
      'Actual: ' + JSON.stringify(actual));
}
domAutomationController.send(isEqual);
