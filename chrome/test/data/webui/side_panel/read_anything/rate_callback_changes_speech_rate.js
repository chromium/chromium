// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.RateCallback_ChangesSpeechRate

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp = document.querySelector('read-anything-app');
  const toolbar =
      readAnythingApp.shadowRoot.querySelector('read-anything-toolbar')
          .shadowRoot;
  const rateMenu = toolbar.getElementById('rateMenu');

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

  const buttons = Array.from(rateMenu.querySelectorAll('.dropdown-item'));
  const rateIcon = toolbar.getElementById('rate');
  let previousRate = -1;
  buttons.forEach((button) => {
    button.click();
    const newRate = readAnythingApp.rate;

    assertGT(newRate, previousRate);
    assertEquals(rateIcon.getAttribute('iron-icon'), 'voice-rate:' + newRate);

    previousRate = newRate;
  });

  return result;
})();
