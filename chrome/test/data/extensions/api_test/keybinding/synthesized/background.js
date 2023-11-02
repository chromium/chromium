// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Commands API test.
//
// Tests the chrome.commands.getAll API returns sane values and that an
// extension with a browser action gets a synthesized extension command with
// an inactive shortcut.
//
// Run with browser_tests:
//     --gtest_filter=CommandsApiTest.SynthesizedCommand

// Called when the user clicks on the browser action.
chrome.browserAction.onClicked.addListener(function(windowId) {
});

chrome.commands.getAll(function(commands) {
  chrome.test.assertEq(2, commands.length);

  // A browser actions gets a synthesized command with no shortcut and no
  // description.
  chrome.test.assertEq("_execute_browser_action", commands[0].name);
  chrome.test.assertEq("",                        commands[0].description);
  chrome.test.assertEq("",                        commands[0].shortcut);

  // This one on the other hand, has it all.
  chrome.test.assertEq("unrelated-feature",       commands[1].name);
  chrome.test.assertEq("Toggle feature foo",      commands[1].description);
  if (window.navigator.platform == "MacIntel") {
    chrome.test.assertEq("⌃⇧Y",                   commands[1].shortcut);
  } else {
    chrome.test.assertEq("Ctrl+Shift+Y",          commands[1].shortcut);
  }

  chrome.test.notifyPass();
});
