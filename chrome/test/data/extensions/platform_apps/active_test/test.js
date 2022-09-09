// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test gets sent commands to execute, which it is sent by the
// controlling C++ code. This code then checks that the apps' active state
// is being tracked correctly.
var windows = [];

function windowClosed() {
  processNextCommand();
}

function processNextCommand() {
  chrome.test.sendMessage("ready", function(response) {
    if (response == 'exit')
      return;

    if (response == 'closeLastWindow') {
      windowToClose = windows.pop();
      windowToClose.close();
      return;
    }

    // Otherwise we are creating a window.
    createOptions = {};

    if (response == 'createMinimized')
      createOptions.state = 'minimized';

    if (response == 'createHidden')
      createOptions.hidden = true;

    chrome.app.window.create('empty.html', createOptions,
        function(createdWindow) {
      createdWindow.onClosed.addListener(windowClosed);
      windows.push(createdWindow);
      processNextCommand();
    });
  });
}

processNextCommand();