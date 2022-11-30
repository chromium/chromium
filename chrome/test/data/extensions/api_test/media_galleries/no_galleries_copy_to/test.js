// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var mediaFileSystemsListCallback = function(results) {
  chrome.test.assertEq(0, results.length);
};

chrome.test.runTests([
  function mediaGalleriesNoGalleries() {
    mediaGalleries.getMediaFileSystems(
        chrome.test.callbackPass(mediaFileSystemsListCallback));
  },
]);
