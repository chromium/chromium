/*
 * Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Tests that tapping the hide keyboard button hides the keyboard.
 * @param {Function} testDoneCallback The callback function on completion.
 */
function testHideKeyboard(testDoneCallback) {
  onKeyboardReady(function() {
    var key = findKeyById('HideKeyboard');
    mockTap(key);
    chrome.virtualKeyboardPrivate.hideKeyboard.addExpectation();
    chrome.virtualKeyboardPrivate.lockKeyboard.addExpectation(false);
    testDoneCallback();
  });
}
