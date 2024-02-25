// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Do not test orientation or hover attributes (similar to exclusions on native
// accessibility), since they can be inconsistent depending on the environment.
var RemoveUntestedStates = function(state) {
  var result = structuredClone(state);
  delete result[StateType.HORIZONTAL];
  delete result[StateType.HOVERED];
  delete result[StateType.VERTICAL];
  return result;
};

var allTests = [
  function testSimplePage() {
    var title = rootNode.docTitle;
    assertEq('Automation Tests', title);
    assertTrue(rootNode.state.focusable);
    assertEq(undefined, rootNode.restriction);

    var children = rootNode.children;
    assertEq(RoleType.ROOT_WEB_AREA, rootNode.role);
    assertEq(1, children.length);
    var body = children[0];
    state = RemoveUntestedStates(body.state);
    assertEq({}, state);
    assertEq(undefined, body.restriction);

    var contentChildren = body.children;
    assertEq(3, contentChildren.length);
    var okButton = contentChildren[0];
    assertEq('Ok', okButton.name);
    state = RemoveUntestedStates(okButton.state);
    assertEq({focusable: true}, state);
    assertEq(undefined, okButton.restriction);
    var userNameInput = contentChildren[1];
    assertEq(undefined, userNameInput.restriction);
    assertEq('Username', userNameInput.name);
    state = RemoveUntestedStates(userNameInput.state);
    assertEq({editable: true, focusable: true}, state);
    var cancelButton = contentChildren[2];
    assertEq('Cancel',
             cancelButton.name);
    state = RemoveUntestedStates(cancelButton.state);
    assertEq({focusable: true}, state);
    assertEq(undefined, cancelButton.restriction);

    // Traversal.
    assertTrue(!!rootNode.parent);
    assertEq(rootNode, body.parent);

    assertEq(body, rootNode.firstChild);
    assertEq(body, rootNode.lastChild);

    assertEq(okButton, body.firstChild);
    assertEq(cancelButton, body.lastChild);

    assertEq(body, okButton.parent);
    assertEq(body, userNameInput.parent);
    assertEq(body, cancelButton.parent);

    assertEq(undefined, okButton.previousSibling);
    assertEq({}, okButton.firstChild);
    assertEq(userNameInput, okButton.nextSibling);
    assertEq({}, okButton.lastChild);

    assertEq(okButton, userNameInput.previousSibling);
    assertEq(cancelButton, userNameInput.nextSibling);

    assertEq(userNameInput, cancelButton.previousSibling);
    assertEq(undefined, cancelButton.nextSibling);

    chrome.test.succeed();
  },
  function testIsRoot() {
    assertTrue(rootNode.isRootNode);
    assertFalse(rootNode.firstChild.isRootNode);
    chrome.test.succeed();
  }
];

setUpAndRunTabsTests(allTests);
