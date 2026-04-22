// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [
  function testSimpleAction() {
    const okButton = rootNode.firstChild.firstChild;
    okButton.addEventListener(EventType.FOCUS, function() {
      chrome.test.succeed();
    }, true);
    okButton.focus();
  },

  function testSetValue() {
    const textField = rootNode.find({role: RoleType.TEXT_FIELD});
    textField.addEventListener(EventType.VALUE_CHANGED, function() {
      assertEq('success!', textField.value);
      chrome.test.succeed();
    }, true);
    textField.setValue('success!');
  },
];

setUpAndRunTabsTests(allTests);
