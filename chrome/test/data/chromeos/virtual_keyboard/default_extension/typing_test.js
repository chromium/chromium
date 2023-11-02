/*
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Tests that typing characters on the default lowercase keyboard triggers the
 * correct sequence of events. The test is run asynchronously since the
 * keyboard loads keysets dynamically.
 */
function testLowercaseKeysetAsync(testDoneCallback) {
  onKeyboardReady(function() {
    // Keyboard defaults to lowercase.
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    mockTypeCharacter('s', 0x53, Modifier.NONE);
    mockTypeCharacter('.', 0, Modifier.NONE);
    mockTypeCharacter('Enter', 0x0D, Modifier.NONE, 0x0D);
    mockTypeCharacter('Space', 0x20, Modifier.NONE, 0x20);
    testDoneCallback();
  });
}

/**
 * When typing quickly, one can often press a second key before releasing the
 * first. This test confirms that both keys are typed in the correct order.
 */
function testStaggeredTypingAsync(testDoneCallback) {
  onKeyboardReady(function() {
    var firstKey = findKey('a');
    var secondKey = findKey('s');
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    var addExpectationsForKeyTap = function(character) {
      var unicodeValue = character.charCodeAt(0);
      // keyCode conversion assumes typing a lowercase alpha character.
      var keyCode = unicodeValue - 0x20;
      send.addExpectation({
        type: 'keydown',
        charValue: unicodeValue,
        keyCode: keyCode,
        modifiers: Modifier.NONE
      });
      send.addExpectation({
        type: 'keyup',
        charValue: unicodeValue,
        keyCode: keyCode,
        modifiers: Modifier.NONE
      });
    };
    mockTouchEvent(firstKey, 'touchstart');
    mockTouchEvent(secondKey, 'touchstart');
    mockTouchEvent(firstKey, 'touchend');
    mockTouchEvent(secondKey, 'touchend');
    addExpectationsForKeyTap('a');
    addExpectationsForKeyTap('s');
    testDoneCallback();
  });
}
