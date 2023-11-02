// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;

var testStep = [
  function testNonDefaultConflictResolutionPolicy() {
    chrome.syncFileSystem.setConflictResolutionPolicy(
        'manual',
        callbackFail('Policy manual is not supported.', testStep.shift()));
  },
  function setConflictResolutionPolicy() {
    chrome.syncFileSystem.setConflictResolutionPolicy(
        'last_write_win', callbackPass(testStep.shift()));
  },
  function getConflictResolutionPolicy() {
    chrome.syncFileSystem.getConflictResolutionPolicy(
        callbackPass(testStep.shift()));
  },
  function checkConflictResolutionPolicy(policy_returned) {
    chrome.test.assertEq('last_write_win', policy_returned);
    chrome.test.succeed();
  }
];

chrome.test.runTests([
  testStep.shift()
]);
