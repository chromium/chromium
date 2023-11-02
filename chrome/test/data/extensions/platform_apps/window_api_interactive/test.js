// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

function testWindowGetsFocus(win) {
  // If the window is already focused, we are done.
  if (win.contentWindow.document.hasFocus()) {
    chrome.test.assertTrue(win.contentWindow.document.hasFocus(),
                           "window has been focused");
    win.close();
    return;
  }

  // Otherwise, we wait for the focus event.
  win.contentWindow.onfocus = callbackPass(function() {
    chrome.test.assertTrue(win.contentWindow.document.hasFocus(),
                           "window has been focused");
    win.close();
  });
}

function testWindowNeverGetsFocus(win) {
  win.contentWindow.onfocus = function() {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
  };

  if (win.contentWindow.document.hasFocus()) {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
    return;
  };

  if (win.contentWindow.document.readyState == 'complete') {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
    return;
  }

  win.contentWindow.onload = callbackPass(function() {
    chrome.test.assertFalse(win.contentWindow.document.hasFocus(),
                            "window should not be focused");
    win.close();
  });
}

// Test that the window's content size is the same as our inner bounds.
// This has to be an interactive test because contentWindow.innerWidth|Height is
// sometimes 0 in the browser test due to an unidentified race condition.
function testInnerBounds() {
  var innerBounds = {
    width: 300,
    height: 301,
    minWidth: 200,
    minHeight: 201,
    maxWidth: 400,
    maxHeight: 401
  };

  function assertInnerBounds(win) {
    chrome.test.assertEq(300, win.contentWindow.innerWidth);
    chrome.test.assertEq(301, win.contentWindow.innerHeight);

    chrome.test.assertEq(300, win.innerBounds.width);
    chrome.test.assertEq(301, win.innerBounds.height);
    chrome.test.assertEq(200, win.innerBounds.minWidth);
    chrome.test.assertEq(201, win.innerBounds.minHeight);
    chrome.test.assertEq(400, win.innerBounds.maxWidth);
    chrome.test.assertEq(401, win.innerBounds.maxHeight);
  }

  chrome.test.runTests([
    function createFrameChrome() {
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function (win) {
        assertInnerBounds(win);
      }));
    },
    function createFrameNone() {
      chrome.app.window.create('test.html', {
        frame: 'none',
        innerBounds: innerBounds
      }, callbackPass(function (win) {
        assertInnerBounds(win);
      }));
    },
    function createFrameColor() {
      chrome.app.window.create('test.html', {
        frame: {color: '#ff0000'},
        innerBounds: innerBounds
      }, callbackPass(function (win) {
        assertInnerBounds(win);
      }));
    }
  ]);
}

function testCreate() {
  chrome.test.runTests([
    function createUnfocusedWindow() {
      chrome.app.window.create('test.html', {
        innerBounds: { width: 200, height: 200 },
        focused: false
      }, callbackPass(testWindowNeverGetsFocus));
    },
    function createTwiceUnfocused() {
      chrome.app.window.create('test.html', {
        id: 'createTwiceUnfocused', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.app.window.create('test.html', {
            id: 'createTwiceUnfocused', focused: false,
            innerBounds: { width: 200, height: 200 }
          }, callbackPass(testWindowNeverGetsFocus));
        });
      }));
    },
    function createFocusedWindow() {
      chrome.app.window.create('test.html', {
        innerBounds: { width: 200, height: 200 },
        focused: true
      }, callbackPass(testWindowGetsFocus));
    },
    function createDefaultFocusStateWindow() {
      chrome.app.window.create('test.html', {
        innerBounds: { width: 200, height: 200 },
      }, callbackPass(testWindowGetsFocus));
    },
    function createTwiceFocusUnfocus() {
      chrome.app.window.create('test.html', {
        id: 'createTwiceFocusUnfocus', focused: true,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.app.window.create('test.html', {
            id: 'createTwiceFocusUnfocus', focused: false,
            innerBounds: { width: 200, height: 200 }
          }, callbackPass(testWindowGetsFocus));
        });
      }));
    },
    function createTwiceUnfocusFocus() {
      chrome.app.window.create('test.html', {
        id: 'createTwiceUnfocusFocus', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.app.window.create('test.html', {
            id: 'createTwiceUnfocusFocus', focused: true,
            innerBounds: { width: 200, height: 200 }
          }, callbackPass(function() {
            // This test fails on Linux GTK, see http://crbug.com/325219
            // And none of those tests run on Linux Aura, see
            // http://crbug.com/325142
            // We remove this and disable the entire test for Linux GTK when the
            // test will run on other platforms, see http://crbug.com/328829
            if (navigator.platform.indexOf('Linux') != 0)
              testWindowGetsFocus(win);
          }));
        });
      }));
    },
  ]);
}

function testShow() {
  chrome.test.runTests([
    function createUnfocusThenShow() {
      chrome.app.window.create('test.html', {
        id: 'createUnfocusThenShow', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          win.show();
          // This test fails on Linux GTK, see http://crbug.com/325219
          // And none of those tests run on Linux Aura, see
          // http://crbug.com/325142
          // We remove this and disable the entire test for Linux GTK when the
          // test will run on other platforms, see http://crbug.com/328829
          if (navigator.platform.indexOf('Linux') != 0)
            testWindowGetsFocus(win);
        });
      }));
    },
    function createUnfocusThenShowUnfocused() {
      chrome.app.window.create('test.html', {
        id: 'createUnfocusThenShowUnfocused', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          win.show(false);
          testWindowNeverGetsFocus(win);
        });
      }));
    },
    function createUnfocusThenShowFocusedThenShowUnfocused() {
      chrome.app.window.create('test.html', {
        id: 'createUnfocusThenShowFocusedThenShowUnfocused', focused: false,
        innerBounds: { width: 200, height: 200 }
      }, callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          win.show(true);
          win.show(false);
          // This test fails on Linux GTK, see http://crbug.com/325219
          // And none of those tests run on Linux Aura, see
          // http://crbug.com/325142
          // We remove this and disable the entire test for Linux GTK when the
          // test will run on other platforms, see http://crbug.com/328829
          if (navigator.platform.indexOf('Linux') != 0)
            testWindowGetsFocus(win);
        });
      }));
    },
  ]);
}

function testDrawAttention() {
  chrome.test.runTests([
    function drawThenClearAttention() {
      chrome.app.window.create('test.html', {}, callbackPass(function(win) {
        win.drawAttention();
        win.clearAttention();
      }));
    }
  ]);
}

function testFullscreen() {
  chrome.test.runTests([
    function createFullscreen() {
      chrome.app.window.create('test.html', {
        state: 'fullscreen',
      }, callbackPass(function (win) {
        chrome.test.assertEq(win.contentWindow.screen.width,
                             win.contentWindow.innerWidth);
        chrome.test.assertEq(win.contentWindow.screen.height,
                             win.contentWindow.innerHeight);
      }));
    }
  ]);
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched', function(reply) {
    window[reply]();
  });
});
