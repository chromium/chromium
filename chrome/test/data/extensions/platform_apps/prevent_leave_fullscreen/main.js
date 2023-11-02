// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('main.html', {}, function(win) {
    // The following key events handler will prevent the default behavior for
    // the ESC key, thus will prevent the ESC key to leave fullscreen.
    win.contentWindow.document.addEventListener('keydown', function(e) {
      e.preventDefault();
    });
    win.contentWindow.document.addEventListener('keyup', function(e) {
      e.preventDefault();
    });

    chrome.test.sendMessage('Launched', function(reply) {
      var doc = win.contentWindow.document;
      doc.addEventListener('keydown', function(e) {
        if (e.keyCode != 90) // 'z'
          return;
        chrome.test.sendMessage('KeyReceived');
      });

      switch (reply) {
        case 'window':
          doc.addEventListener('keydown', function(e) {
            if (e.keyCode != 66) // 'b'
              return;
            doc.removeEventListener('keydown', arguments.callee);
            // We do one trip to the event loop to increase the chances that
            // fullscreen could have been left before the message is received.
            setTimeout(function() {
              chrome.test.sendMessage('B_KEY_RECEIVED');
            });
          });
          win.fullscreen();
          break;

        case 'dom':
          doc.addEventListener('keydown', function() {
            doc.removeEventListener('keydown', arguments.callee);

            doc.addEventListener('keydown', function(e) {
              if (e.keyCode != 66) // 'b'
                return;
              doc.removeEventListener('keydown', arguments.callee);
              // We do one trip to the event loop to increase the chances that
              // fullscreen could have been left before the message is received.
              setTimeout(function() {
                chrome.test.sendMessage('B_KEY_RECEIVED');
              });
            });

            doc.body.webkitRequestFullscreen();
          });
          break;
      }
    });
  });
});
