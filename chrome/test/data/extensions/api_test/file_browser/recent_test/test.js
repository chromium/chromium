// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test files should be created before running the test extension.
 */

function getVolumeMetadataList() {
  return new Promise(function(resolve, reject) {
    chrome.fileManagerPrivate.getVolumeMetadataList(resolve);
  });
}

function requestFileSystem(volumeId) {
  return new Promise(function(resolve, reject) {
    chrome.fileSystem.requestFileSystem(
        {volumeId: volumeId},
        function(fileSystem) {
          if (!fileSystem) {
            reject(new Error('Failed to acquire volume.'));
          }
          resolve(fileSystem);
        });
  });
}

function requestAllFileSystems() {
  return getVolumeMetadataList().then(function(volumes) {
    return Promise.all(volumes.map(function(volume) {
      return requestFileSystem(volume.volumeId);
    }));
  });
}

// Run the tests.
requestAllFileSystems().then(function() {
  function exists(entries, fileName) {
    for (let i = 0; i < entries.length; ++i) {
      if (entries[i].name === fileName) {
        return true;
      }
    }
    return false;
  }
  chrome.test.runTests([
    function testGetRecentFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'all', chrome.test.callbackPass(entries => {
            chrome.test.assertTrue(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg not found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 not found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 not found');
          }));
    },
    function testGetRecentAudioFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'audio', chrome.test.callbackPass(entries => {
            chrome.test.assertFalse(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg unexpectedly found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 not found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 unexpectedly found');
          }));
    },
    function testGetRecentImageFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'image', chrome.test.callbackPass(entries => {
            chrome.test.assertTrue(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg not found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 unexpectedly found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 unexpectedly found');
          }));
    },
    function testGetRecentVideoFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'video', chrome.test.callbackPass(entries => {
            chrome.test.assertFalse(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg unexpectedly found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 unexpectedly found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 not found');
          }));
    }
  ]);
});
