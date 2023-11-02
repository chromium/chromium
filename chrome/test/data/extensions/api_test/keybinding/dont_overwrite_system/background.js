// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user activates the command.
chrome.commands.onCommand.addListener(function(command) {
  chrome.test.sendMessage(command);
});

chrome.test.notifyPass();
