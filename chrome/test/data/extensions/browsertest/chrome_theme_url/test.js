// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.test.runTests([
    function theme() {
      var img = document.getElementById('theme');
      var loaded = img.naturalWidth == 16 && img.naturalHeight == 16;
      chrome.test.sendMessage(loaded ? 'loaded' : 'not loaded');
    }
  ]);
};
