// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var panelWindow;
var nonShelfWindow;
var shelfWindow;

function processNextCommand() {
  chrome.test.sendMessage('ready', function(response) {
    if (response == 'exit') {
      return;
    }
    if (response == 'createPanelWindow') {
      chrome.app.window.create('main.html', { type: 'panel' }, function (win) {
         panelWindow = win;
         // To avoid race condition get next command only after the window is
         // actually created.
         processNextCommand();
      });
    } else if (response == 'setPanelWindowIcon') {
      panelWindow.setIcon('icon64.png')
      processNextCommand();
    } else if (response == 'createNonShelfWindow') {
      // Create the shell window; it should use the app icon, and not affect
      // the panel icon.
      chrome.app.window.create(
        'main.html', { id: 'win',
                       type: 'shell' },
        function (win) {
            nonShelfWindow = win;
            processNextCommand();
        });
    } else if (response == 'createShelfWindow') {
      // Create the shell window which is shown in shelf; it should use the
      // default custom app icon.
      chrome.app.window.create(
        'main.html', { id: 'win_with_icon',
                       type: 'shell',
                       showInShelf: true },
        function (win) {
          shelfWindow = win;
          processNextCommand();
        });
    } else if (response == 'setShelfWindowIcon') {
      shelfWindow.setIcon('icon32.png')
      processNextCommand();
    } else if (response == 'createShelfWindowWithCustomIcon') {
      // Create the shell window which is shown in shelf; it should use
      // another custom app icon.
      chrome.app.window.create(
        'main.html', { id: 'win_with_custom_icon',
                       type: 'shell',
                       icon: 'icon32.png',
                       showInShelf: true },
        function (win) {
          processNextCommand();
       });
    } else {
      console.error('Unrecognized command: ' + response);
    }
  });
};


chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched');
  processNextCommand();
});

