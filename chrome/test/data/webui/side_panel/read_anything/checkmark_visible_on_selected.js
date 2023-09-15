// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.Checkmarks_Visible

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
  const assertCheckMarkVisible = (checkMarks, expectedIndex) => {
    checkMarks.forEach((element, index) => {
      if (index === expectedIndex) {
        assertEquals(element.style.visibility, 'visible');
      } else {
        assertEquals(element.style.visibility, 'hidden');
      }
    });
  };
  // Check that the check mark of the selected item is visible
  const assertCheckMarksForDropdown = (dropdown) => {
    const buttons = Array.from(dropdown.querySelectorAll('.dropdown-item'));
    const checkMarks = Array.from(dropdown.querySelectorAll('.check-mark'));
    buttons.forEach((button, index) => {
      button.click();
      assertCheckMarkVisible(checkMarks, index);
    });
  };

  assertCheckMarksForDropdown(toolbar.getElementById('fontMenu'));
  assertCheckMarksForDropdown(toolbar.getElementById('rateMenu'));
  assertCheckMarksForDropdown(toolbar.getElementById('lineSpacingMenu'));
  assertCheckMarksForDropdown(toolbar.getElementById('letterSpacingMenu'));
  assertCheckMarksForDropdown(toolbar.getElementById('colorMenu'));

  return result;
})();
