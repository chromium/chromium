// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These string constants should be consistent with those in
// chrome/browser/extensions/api/command_line_private/.
var kTestCommandLineSwitch = 'command-line-private-api-test-foo';
var kEmptySwitchName = 'Switch name is empty.';

var kNonExistingSwitch = 'foo-bar-non-existing-switch';

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;

chrome.test.runTests([

  function testHaveSwitch() {
    chrome.commandLinePrivate.hasSwitch(kTestCommandLineSwitch,
        pass(function(result) {
      assertTrue(result);
    }));
  },

  function testNotHaveSwitch() {
    chrome.commandLinePrivate.hasSwitch(kNonExistingSwitch,
        pass(function(result) {
      assertFalse(result);
    }));
  },

  function testInvalidArgs() {
    chrome.commandLinePrivate.hasSwitch('', fail(kEmptySwitchName));
  }

]);
