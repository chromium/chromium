// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.ReadAnythingToolbar_Visible

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const toolbar = readAnythingApp.getElementById('toolbar-container');

  const expected = false;
  const actual = toolbar.hidden;
  const isEqual = actual === expected;
  if (!isEqual) {
    console.error(
        'Expected: ' + JSON.stringify(expected) + ', ' +
        'Actual: ' + JSON.stringify(actual));
  }
  return isEqual;
})();
