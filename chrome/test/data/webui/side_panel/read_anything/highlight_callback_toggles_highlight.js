// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.HighlightCallback_TogglesHighlight
// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};
  const readAnythingApp = document.querySelector('read-anything-app');
  const toolbar =
      readAnythingApp.shadowRoot.querySelector('read-anything-toolbar')
          .shadowRoot;
  const highlightButton = toolbar.getElementById('highlight');
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

  // TODO(b/1474951): Test that the actual functionality is toggled too
  assertEquals(
      highlightButton.getAttribute('iron-icon'), 'read-anything:highlight-on');
  highlightButton.click();
  assertEquals(
      highlightButton.getAttribute('iron-icon'), 'read-anything:highlight-off');
  highlightButton.click();
  assertEquals(
      highlightButton.getAttribute('iron-icon'), 'read-anything:highlight-on');
  return result;
})();
