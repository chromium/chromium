// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error =
    "Extensions require the \"app.window.ime\" permission to create windows.";

function testImeEnabled(imeValue) {
  var createOptions = { frame: 'none' };
  createOptions.ime = imeValue;

  chrome.app.window.create('index.html',
                           createOptions,
                           chrome.test.callbackFail(error));
}

// All these tests are run without app.window.ime permission set and on a system
// with ime window support.
chrome.test.runTests([

  // Window is created with ime set to true.
  // Expect fail.
  function testImeNoPermissionImeInitTrue() {
    testImeEnabled(true);
  },

  // Window is created with ime set to false.
  // Expect fail.
  function testImeNoPermissionImeInitFalse() {
    testImeEnabled(false);
  }

]);
