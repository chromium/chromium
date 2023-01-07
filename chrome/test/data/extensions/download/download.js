// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  // Begin the download (Code based on IE Tab extension)
  var anchorObj = document.body.children.namedItem('helper-download');
  anchorObj.href = chrome.runtime.getURL('download.dat');
  // Raise a fake click on the .dat file link, which will rename it to .exe
  var evt = document.createEvent("MouseEvents");
  evt.initMouseEvent("click", true, true, window,
    0, 0, 0, 0, 0, false, false, false, false, 0, null);
  var allowDefault = anchorObj.dispatchEvent(evt);
};
