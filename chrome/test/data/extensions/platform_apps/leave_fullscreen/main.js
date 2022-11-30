// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('main.html', {}, function(win) {
    // The following key events handler should have no effect because the
    // application does not have the 'overrideEscFullscreen' permission.
    win.contentWindow.document.addEventListener('keydown', function(e) {
      e.preventDefault();
    });
    win.contentWindow.document.addEventListener('keyup', function(e) {
      e.preventDefault();
    });

    chrome.test.sendMessage('Launched', function(reply) {
      win.contentWindow.document.addEventListener('keydown', function(e) {
        if (e.keyCode != 90) // 'z'
          return;

        chrome.test.sendMessage('KeyReceived');
      });

      switch (reply) {
        case 'window':
          win.fullscreen();
          break;
        case 'dom':
          win.contentWindow.document.addEventListener('keydown', function() {
            win.contentWindow.document.removeEventListener('keydown',
                                                           arguments.callee);
            win.contentWindow.document.body.webkitRequestFullscreen();
          });
          break;
      }
    });
  });
});
