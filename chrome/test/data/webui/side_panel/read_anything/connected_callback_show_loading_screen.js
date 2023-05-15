// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.ConnectedCallback_ShowLoadingScreen

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readAnything.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const emptyState = readAnythingApp.querySelector('sp-empty-state');
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

  const assertStringContains = (string, value) => {
    const contains = string.includes(value);
    if (!contains) {
      console.error(
          'Expected to find: ' + JSON.stringify(value) + ', ' +
          'Actual: ' + JSON.stringify(string));
    }
    result = result && contains;
    return contains;
  };

  assertEquals(
      readAnythingApp.getElementById('empty-state-container').hidden, false);
  assertEquals(emptyState.heading, 'Getting ready');
  assertEquals(emptyState.body, '');
  assertStringContains(emptyState.imagePath, 'throbber');
  assertStringContains(emptyState.darkImagePath, 'throbber');
  return result;
})();
