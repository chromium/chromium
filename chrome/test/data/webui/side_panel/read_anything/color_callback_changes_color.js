// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppToolbarTest.ColorCallback_ChangesColor

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp = document.querySelector('read-anything-app');
  const toolbar =
      readAnythingApp.shadowRoot.querySelector('read-anything-toolbar')
          .shadowRoot;
  const container = readAnythingApp.shadowRoot.getElementById('container');
  const colorMenu = toolbar.getElementById('colorMenu');

  let result = true;

  const assertNE = (actual, expected) => {
    const isNotEqual = actual !== expected;
    if (!isNotEqual) {
      console.error(
          'Expected ' + JSON.stringify(actual) + ' to be not equal to ' +
          +JSON.stringify(expected));
    }
    result = result && isNotEqual;
    return isNotEqual;
  };

  // The actual background and foreground colors we use are color constants
  // the test doesn't have access to, so set up colors to verify each button
  // makes a different change. Changing color themes changes more than just
  // these colors, but these are the most important, and testing every single
  // type of color change is brittle
  container.style.setProperty('--bg', 'transparent');
  container.style.setProperty('--bg-dark', 'black');
  container.style.setProperty('--bg-light', 'white');
  container.style.setProperty('--bg-yellow', 'yellow');
  container.style.setProperty('--bg-blue', 'blue');
  readAnythingApp.getBackgroundColorVar = (colorSuffix) => {
    return container.style.getPropertyValue(`--bg${colorSuffix}`);
  };
  container.style.setProperty('--fg', 'purple');
  container.style.setProperty('--fg-dark', 'white');
  container.style.setProperty('--fg-light', 'black');
  container.style.setProperty('--fg-yellow', 'blue');
  container.style.setProperty('--fg-blue', 'yellow');
  readAnythingApp.getForegroundColorVar = (colorSuffix) => {
    return container.style.getPropertyValue(`--fg${colorSuffix}`);
  };

  const buttons = Array.from(colorMenu.querySelectorAll('.dropdown-item'));
  let previousBackground = '';
  let previousForeground = '';
  buttons.forEach((button) => {
    button.click();
    const newBackground = (window.getComputedStyle(container).backgroundColor);
    const newForeground = (window.getComputedStyle(container).color);

    assertNE(newBackground, previousBackground);
    assertNE(newForeground, previousForeground);

    previousBackground = newBackground;
    previousForeground = newForeground;
  });

  return result;
})();
