// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var failToSendKeyEvents = 'Could not send key events';

chrome.test.runTests([
  // Tests input.ime.activate and input.ime.onFocus APIs.
  function testActivateAndFocus() {
    var focused = false;
    var activated = false;
    chrome.input.ime.onFocus.addListener(function(context) {
      if (context.type == 'none') {
        chrome.test.fail();
        return;
      }
      focused = true;
      if (activated)
        chrome.test.succeed();
    });
    chrome.input.ime.activate(function() {
      if (chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      activated = true;
      if (focused)
        chrome.test.succeed();
    });
  },
  // Test input.ime.createWindow API.
  function testNormalCreateWindow() {
    var options = { windowType: 'normal' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertNoLastError()
      chrome.test.assertTrue(!!win);
      chrome.test.assertTrue(win instanceof Window);
      chrome.test.assertFalse(win.document.webkitHidden);
      // Test for security origin.
      // If security origin is not correctly set, there will be securtiy
      // exceptions when accessing DOM or add event listeners.
      win.addEventListener('unload', function() {});
      chrome.test.succeed();
    });
  },
  function testFollowCursorCreateWindow() {
    var options = { windowType: 'followCursor' };
    chrome.input.ime.createWindow(options, function(win) {
      chrome.test.assertNoLastError()
      chrome.test.assertTrue(!!win);
      chrome.test.assertTrue(win instanceof Window);
      chrome.test.assertFalse(win.document.webkitHidden);
      // test for security origin.
      // If security origin is not correctly set, there will be securtiy
      // exceptions when accessing DOM or add event listeners.
      win.addEventListener('unload', function() {});
      chrome.test.succeed();
    });
  },
  function testCommitText() {
    chrome.input.ime.commitText({
      contextID: 1,
      text: 'test_commit_text'
    }, function () {
      if (chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      chrome.test.succeed();
    });
  },
  // Tests input.ime.activate and input.ime.setComposition API.
  function testSetComposition() {
    chrome.input.ime.setComposition({
      contextID: 1,
      text: 'test_set_composition',
      cursor: 2
    }, function() {
      if(chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      chrome.test.succeed();
    });
  },
  function testEmptyComposition() {
    chrome.input.ime.setComposition({
      contextID: 1,
      text: '',
      cursor: 0
    }, function() {
      if(chrome.runtime.lastError) {
        chrome.test.fail();
        return;
      }
      chrome.test.succeed();
    });
  },
  // Test input.ime.sendKeyEvents API.
  function testSendKeyEvents() {
    // Sends a normal character key.
    chrome.input.ime.sendKeyEvents({
      contextID: 1,
      keyData: [{
        type: 'keydown',
        key: 'a',
        code: 'KeyA'
      }, {
        type: 'keyup',
        key: 'a',
        code: 'KeyA',
        // Check backwards-compatibility: should be able to put a requestID.
        // See crbug.com/1005996.
        requestId: '1'
     }]
    }, function() {
      // Normal character key should be allowed to send on any page.
      chrome.test.assertNoLastError();
    });
    // Sends Ctrl+A that should fail on special pages.
    chrome.input.ime.sendKeyEvents({
      contextID: 1,
      keyData: [{
        type: 'keydown',
        key: 'a',
        code: 'KeyA',
        ctrlKey: true
      }, {
        type: 'keyup',
        key: 'a',
        code: 'KeyA',
        ctrlKey: true
     }]
    }, function() {
      if (chrome.runtime.lastError) {
        chrome.test.assertEq(failToSendKeyEvents,
                             chrome.runtime.lastError.message);
      }
    });
    // Sends Tab key that should fail on special pages.
    chrome.input.ime.sendKeyEvents({
      contextID: 1,
      keyData: [{
        type: 'keydown',
        key: '\u0009', // Unicode value for Tab key.
        code: 'Tab'
      }]
    }, function() {
      if (chrome.runtime.lastError) {
        chrome.test.assertEq(failToSendKeyEvents,
                             chrome.runtime.lastError.message);
      }
    });
    chrome.test.succeed();
  },
  // Tests input.ime.onBlur API.
  function testBlur() {
    chrome.input.ime.onBlur.addListener(function(context) {
      if (context.type != 'none') {
        // Waits for the 'get_blur_event' message in
        // InputImeApiTest.BasicApiTest.
        chrome.test.sendMessage('get_blur_event');
      }
      chrome.test.succeed();
    });
    chrome.test.succeed();
  }
]);
