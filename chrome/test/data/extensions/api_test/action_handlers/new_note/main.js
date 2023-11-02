// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  var isNewNote = false;
  if (launchData.actionData && launchData.actionData.actionType == "new_note")
    isNewNote = true

  chrome.test.sendMessage('hasNewNote = ' + isNewNote);
});

chrome.test.sendMessage('loaded');
