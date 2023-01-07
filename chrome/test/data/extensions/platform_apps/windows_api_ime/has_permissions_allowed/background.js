// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error = 'IME extensions must create an IME window ( with "ime: true" and ' +
    '"frame: \'none\'"). Panels are no longer supported for IME extensions.';

function testImeEnabled(createOptions) {
  if (createOptions.frame == 'none' && createOptions.ime) {
    chrome.app.window.create('index.html',
                             createOptions,
                             chrome.test.callbackPass(function(win) {}));
  } else if (createOptions.type == 'type'){
    chrome.app.window.create('index.html',
                             createOptions,
                             chrome.test.callbackPass(function(win) {}));
  } else {
    chrome.app.window.create('index.html',
                             createOptions,
                             chrome.test.callbackFail(error));
  }
}

// All these tests are run with app.window.ime permission set and on a system
// with ime window support.
chrome.test.runTests([

  // Window is created with ime set to true and frame set to none.
  // Expect pass.
  function testImeEnabledPermissionImeTrueFrameNone() {
    testImeEnabled({ ime: true, frame: 'none' });
  },

  // Window is created with ime set to false or frame not set.
  // Expect fail.
  function testImeEnabledPermissionImeInitFalse() {
    testImeEnabled({ ime: false, frame: 'none' });
    testImeEnabled({ ime: false });
    testImeEnabled({ ime: true });
  },

  // Window is created with ime not explicitly set.
  // Expect fail.
  function testImeEnabledPermissonImeNoInit() {
    testImeEnabled({ frame: 'none' });
    testImeEnabled({});
  }

]);
