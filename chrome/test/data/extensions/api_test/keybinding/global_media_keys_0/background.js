// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user activates the command.
chrome.commands.onCommand.addListener(function(command) {
  if (command == "MediaNextTrack-global") {
    chrome.test.notifyPass();
    return;
  }

  // Everything else is a failure case.
  chrome.test.notifyFail("Unexpected command received: " + command);
});

chrome.test.notifyPass();
