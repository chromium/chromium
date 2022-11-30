// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function testEval() {
  window.foo = 2;
  eval('window.foo = 3;');
  chrome.test.assertEq(3, window.foo);
  chrome.test.succeed();
}]);
