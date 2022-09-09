// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  function NoAppWindows() {
    if (typeof(chrome.app.window) == "undefined")
      chrome.test.succeed();
    else
      chrome.test.fail("chrome.app.window is defined, but shouldn't be");
  }

]);
