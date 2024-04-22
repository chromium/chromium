// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppTest.\
//    OnSelectionChange_NothingSelectedOnLoadingScreenSelection

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp =
      document.querySelector('read-anything-app').shadowRoot;
  const emptyState = readAnythingApp.getElementById('empty-state-container');

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


  const range = new Range();
  range.setStartBefore(emptyState);
  range.setEndAfter(emptyState);
  const selection = readAnythingApp.getSelection();
  selection.removeAllRanges();
  selection.addRange(range);

  return new Promise(resolve => {
    setTimeout(() => {
      const retrieved_selection = readAnythingApp.getSelection();
      assertEquals(retrieved_selection.rangeCount, 0);
      assertEquals(retrieved_selection.anchorNode, null);
      assertEquals(retrieved_selection.focusNode, null);
      assertEquals(retrieved_selection.anchorOffset, 0);
      assertEquals(retrieved_selection.anchorOffset, 0);
      assertEquals(retrieved_selection.focusOffset, 0);
      resolve(result);
    }, 1000);
  });
})();
