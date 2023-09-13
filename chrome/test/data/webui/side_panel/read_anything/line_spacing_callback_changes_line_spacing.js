// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.LineSpacingCallback_ChangesLineSpacing

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
  const lineSpacingMenu = toolbar.getElementById('lineSpacingMenu');

  let result = true;

  const assertGT = (greater, less) => {
    const isGreater = parseFloat(greater) > parseFloat(less);
    if (!isGreater) {
      console.error(
          'Expected ' + JSON.stringify(greater) + ' to be greater than ' +
          +JSON.stringify(less));
    }
    result = result && isGreater;
    return isGreater;
  };

  const buttons =
      Array.from(lineSpacingMenu.querySelectorAll('.dropdown-item'));
  let previousLineSpacing = -1;
  buttons.forEach((button) => {
    button.click();
    const newLineSpacing =
        parseFloat(window.getComputedStyle(container).lineHeight);
    assertGT(newLineSpacing, previousLineSpacing);
    previousLineSpacing = newLineSpacing;
  });

  return result;
})();
