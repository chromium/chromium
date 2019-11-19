// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let allTests = [
  function testEventListenerTarget() {
    let cancelButton = rootNode.firstChild.children[2];
    assertEq('Cancel', cancelButton.name);
    cancelButton.addEventListener(EventType.FOCUS,
                                  function onFocusTarget(event) {
      window.setTimeout(function() {
        cancelButton.removeEventListener(EventType.FOCUS, onFocusTarget);
        chrome.test.succeed();
      }, 0);
    });
    cancelButton.focus();
  },
  function testEventListenerBubble() {
    let okButton = rootNode.firstChild.children[0];
    assertEq('Ok', okButton.name);
    let okButtonGotEvent = false;
    okButton.addEventListener(EventType.FOCUS,
                                  function onFocusBubble(event) {
      okButtonGotEvent = true;
      okButton.removeEventListener(EventType.FOCUS, onFocusBubble);
    });
    rootNode.addEventListener(EventType.FOCUS,
                               function onFocusBubbleRoot(event) {
      assertEq('focus', event.type);
      assertEq(okButton, event.target);
      assertTrue(okButtonGotEvent);
      rootNode.removeEventListener(EventType.FOCUS, onFocusBubbleRoot);
      chrome.test.succeed();
    });
    okButton.focus();
  },
  function testStopPropagation() {
    let cancelButton = rootNode.firstChild.children[2];
    assertEq('Cancel', cancelButton.name);
    function onFocusStopPropRoot(event) {
      rootNode.removeEventListener(EventType.FOCUS, onFocusStopPropRoot);
      chrome.test.fail("Focus event was propagated to root");
    };
    cancelButton.addEventListener(EventType.FOCUS,
                                  function onFocusStopProp(event) {
      cancelButton.removeEventListener(EventType.FOCUS, onFocusStopProp);
      event.stopPropagation();
      window.setTimeout((function() {
        rootNode.removeEventListener(EventType.FOCUS, onFocusStopPropRoot);
        chrome.test.succeed();
      }).bind(this), 0);
    });
    rootNode.addEventListener(EventType.FOCUS, onFocusStopPropRoot);
    cancelButton.focus();
  },
  function testEventListenerCapture() {
    let okButton = rootNode.firstChild.children[0];
    assertEq('Ok', okButton.name);
    let okButtonGotEvent = false;
    function onFocusCapture(event) {
      okButtonGotEvent = true;
      okButton.removeEventListener(EventType.FOCUS, onFocusCapture);
      chrome.test.fail("Focus event was not captured by root");
    };
    okButton.addEventListener(EventType.FOCUS, onFocusCapture);
    rootNode.addEventListener(EventType.FOCUS,
                               function onFocusCaptureRoot(event) {
      assertEq('focus', event.type);
      assertEq(okButton, event.target);
      assertFalse(okButtonGotEvent);
      event.stopPropagation();
      rootNode.removeEventListener(EventType.FOCUS, onFocusCaptureRoot);
      rootNode.removeEventListener(EventType.FOCUS, onFocusCapture);
      window.setTimeout(chrome.test.succeed.bind(this), 0);
    }, true);
    okButton.focus();
  },
  function testHitTestWithReply() {
    let cancelButton = rootNode.firstChild.children[2];
    assertEq('Cancel', cancelButton.name);
    let loc = cancelButton.unclippedLocation;
    rootNode.hitTestWithReply(loc.left, loc.top, function(result) {
      assertEq(result, cancelButton);
      chrome.test.succeed();
    });
  }
];

setUpAndRunTests(allTests)
