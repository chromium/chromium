// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var deepEq = chrome.test.checkDeepEq;

var EventType = chrome.automation.EventType;
var assertEqualEvent = EventType.LOAD_COMPLETE;
var assertNotEqualEvent = EventType.ACTIVEDESCENDANTCHANGED;
var testCompleteEvent = EventType.BLUR;

var allTests = [
  function testDeserializeGeneratedTrees() {
    var tree0, tree1;
    function onTree1Retrieved(rootNode) {
      tree1 = rootNode;
      tree1.addEventListener('destroyed', function() {
        chrome.automation.getTree(1, onTree1Retrieved);
      });
    }
    chrome.automation.getTree(1, onTree1Retrieved);

    function onTree0Retrieved(rootNode) {
      tree0 = rootNode;
      tree0.addEventListener(assertEqualEvent, function() {
        assertEq(tree0.toString(), tree1.toString(),
                 'tree0 should be equal to tree1 after loadComplete');
        tree0.doDefault();
      });
      tree0.addEventListener(assertNotEqualEvent, function() {
        assertFalse(tree0.toString() == tree1.toString(),
                    'tree0 should not be equal to tree1');
      });
      tree0.addEventListener('destroyed', function() {
        chrome.automation.getTree(0, onTree0Retrieved);
      });
      tree0.addEventListener(testCompleteEvent, function() {
        chrome.test.succeed();
      });
      tree0.doDefault();
    }
    chrome.automation.getTree(0, onTree0Retrieved);
  }
];

chrome.test.runTests(allTests);
