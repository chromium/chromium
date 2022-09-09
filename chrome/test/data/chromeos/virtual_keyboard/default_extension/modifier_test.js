/*
 * Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Test that the Control modifier key is sticky.
 */
function testControlKeyStickyAsync(testDoneCallback) {
  var testCallback = function() {
    mockTap(findKeyById('ControlLeft'));
    mockTypeCharacter('a', 0x41, Modifier.CONTROL, 0);

    // Ensure that the control key is no longer sticking. i.e. Ensure that
    // typing 'a' on its own results in only 'a'.
    mockTypeCharacter('a', 0x41, Modifier.NONE);

    testDoneCallback();
  };
  var config =
      {keyset: 'us', languageCode: 'en', passwordLayout: 'us', name: 'English'};
  onKeyboardReady(testCallback, config);
}

/**
 * Test that holding down a modifier key will apply it to all character keys
 * until it is released.
 */
function testChordedControlKeyAsync(testDoneCallback) {
  var testCallback = function() {
    var controlKey = findKeyById('ControlLeft');
    mockTouchEvent(controlKey, 'touchstart');

    // Expect the first chorded press of Ctrl+a to work.
    mockTypeCharacter('a', 0x41, Modifier.CONTROL, 0);

    // Expect following chorded presses to work as well.
    mockTypeCharacter('a', 0x41, Modifier.CONTROL, 0);

    // Expect a regular tap of a key after chording ends.
    mockTouchEvent(controlKey, 'touchend');
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    testDoneCallback();
  };
  var config =
      {keyset: 'us', languageCode: 'en', passwordLayout: 'us', name: 'English'};
  onKeyboardReady(testCallback, config);
}

/**
 * Test that multiple sticky keys stack until a character is typed.
 */
function testMultipleStickyModifiersAsync(testDoneCallback) {
  var testCallback = function() {
    mockTap(findKeyById('ControlLeft'));
    mockTap(findKeyById('AltLeft'));
    mockTap(findKeyById('ShiftLeft'));
    mockTypeCharacter(
        'A', 0x41, Modifier.CONTROL | Modifier.ALT | Modifier.SHIFT, 0);

    // Keys should un-stick on a subsequent press.
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    testDoneCallback();
  };
  var config =
      {keyset: 'us', languageCode: 'en', passwordLayout: 'us', name: 'English'};
  onKeyboardReady(testCallback, config);
}

/**
 * Test that the second tap on a sticky key disables it.
 */
function testDoubleTapUnstickAsync(testDoneCallback) {
  var testCallback = function() {
    mockTap(findKeyById('ControlLeft'));
    mockTap(findKeyById('ControlLeft'));
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    testDoneCallback();
  };
  var config =
      {keyset: 'us', languageCode: 'en', passwordLayout: 'us', name: 'English'};
  onKeyboardReady(testCallback, config);
}
