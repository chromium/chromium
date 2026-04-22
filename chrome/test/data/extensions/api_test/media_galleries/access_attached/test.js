// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const mediaGalleries = chrome.mediaGalleries;

let galleries;
const testResults = [];
let expectedFileSystems;
let testGalleryName;

const mediaFileSystemsListCallback = function(results) {
  galleries = results;
};

chrome.test.getConfig(function(config) {
  const customArg = JSON.parse(config.customArg);
  expectedFileSystems = customArg[0];
  testGalleryName = customArg[1];

  chrome.test.runTests([
    function mediaGalleriesAccessAttached() {
      mediaGalleries.getMediaFileSystems(
          chrome.test.callbackPass(mediaFileSystemsListCallback));
    },
    function testGalleries() {
      chrome.test.assertEq(expectedFileSystems, galleries.length);

      for (let i = 0; i < galleries.length; i++) {
        const metadata =
            mediaGalleries.getMediaFileSystemMetadata(galleries[i]);
        if (metadata.name == testGalleryName) {
          chrome.test.succeed();
          return;
        } else {
          testResults.push(metadata.name);
        }
      }
      chrome.test.fail(`${testResults} vs ${testGalleryName}`);
    },
  ]);
});
