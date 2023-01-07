// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A list of all commands sorted in expected order.
var expectedCommands = [
  'Search-Shift-Left',
  'Search-Shift-Up',
  'Search-Shift-Right',
  'Search-Shift-Down'
];

chrome.commands.onCommand.addListener(function (command) {
  if (expectedCommands[0] != command)
    chrome.test.notifyFail('Unexpected command: ' + command);
  expectedCommands.splice(0, 1);
  if (expectedCommands.length == 0)
    chrome.test.notifyPass();
});

chrome.test.notifyPass();
