// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


window.onload = function() {

  chrome.test.runTests([
    function favicon() {
      var img = document.getElementById('favicon');
      chrome.test.assertEq(16, img.naturalWidth);
      chrome.test.assertEq(16, img.naturalHeight);
      chrome.test.succeed();
    },

    function theme() {
      var img = document.getElementById('theme');
      chrome.test.assertEq(0, img.naturalWidth);
      chrome.test.assertEq(0, img.naturalHeight);
      chrome.test.succeed();
    }
  ]);

};
