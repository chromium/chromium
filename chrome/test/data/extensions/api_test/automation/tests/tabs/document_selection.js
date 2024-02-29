// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var html = "<head><title>testdoc</title></head>" +
    '<p>para1</p><input type="text" id="textField" value="hello world">';

var allTests = [
  function testInitialSelectionNotSet() {
    assertEq(undefined, rootNode.anchorObject);
    assertEq(undefined, rootNode.anchorOffset);
    assertEq(undefined, rootNode.focusObject);
    assertEq(undefined, rootNode.focusOffset);
    chrome.test.succeed();
  },

  function selectOutsideTextField() {
    var textNode = rootNode.find({role: RoleType.PARAGRAPH}).firstChild;
    assertTrue(!!textNode);
    chrome.automation.setDocumentSelection({anchorObject: textNode,
                                            anchorOffset: 0,
                                            focusObject: textNode,
                                            focusOffset: 3});
    listenOnce(rootNode, EventType.DOCUMENT_SELECTION_CHANGED, function(evt) {
      assertEq(textNode, rootNode.anchorObject);
      assertEq(0, rootNode.anchorOffset);
      assertEq(textNode, rootNode.focusObject);
      assertEq(3, rootNode.focusOffset);
      chrome.test.succeed();
    });
  },

  function selectInTextField() {
    var textField = rootNode.find({role: RoleType.TEXT_FIELD});
    textField.focus();
    assertTrue(!!textField);
    // Focusing the textfield will cause a text selection changed event in it.
    listenOnce(textField, EventType.TEXT_SELECTION_CHANGED, function(evt2) {
      assertTrue(evt2.target == textField);
      assertEq(textField, rootNode.anchorObject);
      assertEq(0, rootNode.anchorOffset);
      assertEq(textField, rootNode.focusObject);
      assertEq(0, rootNode.focusOffset);

      // Setting selection within the textfield causes a document selection and
      // a textfield selection event.
      chrome.automation.setDocumentSelection({anchorObject: textField,
                                              anchorOffset: 1,
                                              focusObject: textField,
                                              focusOffset: 3});
      listenOnce(rootNode, EventType.DOCUMENT_SELECTION_CHANGED,
                 function(evt) {
        listenOnce(textField, EventType.TEXT_SELECTION_CHANGED,
                   function(evt) {
          assertEq(textField, rootNode.anchorObject);
          assertEq(1, rootNode.anchorOffset);
          assertEq(textField, rootNode.focusObject);
          assertEq(3, rootNode.focusOffset);
          chrome.test.succeed();
        });
      });
    });
  },
];

setUpAndRunTabsTests(allTests, 'document_selection.html');
